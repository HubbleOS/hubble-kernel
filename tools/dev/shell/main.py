import time
import curses
import subprocess
import json
import sys
import os
import tomllib  # Python 3.11+


# ----------- Config & Menu -----------

class Config:
    def __init__(self, path="config.toml"):
        self.path = path
        self.data = self.load()

    def load(self):
        if not os.path.exists(self.path):
            return {}
        with open(self.path, "rb") as f:
            return tomllib.load(f)

    def get_params(self, key):
        key = key.lower().replace(" ", "_")
        return self.data.get(key, {})


class Menu:
    def __init__(self, path="menu.json"):
        self.path = path
        self.items = self.load()

    def load(self):
        if not os.path.exists(self.path):
            return []
        with open(self.path) as f:
            data = json.load(f)
        return [(item["name"], item["cmd"], item.get("args", {}), item["desc"], item.get("interactive", False))
                for item in data]

    def get_by_name(self, name):
        for item in self.items:
            if item[0] == name:
                return item
        return None


# ----------- Command Runner -----------

def run_command(cmd, log_win=None, interactive=False, cfg_params=None, menu_args=None):
    all_params = {}
    if menu_args:
        all_params.update(menu_args)  # берём args из меню
    if cfg_params:
        all_params.update(cfg_params)  # поверх подставляем из конфигурации

    if all_params:
        try:
            cmd = cmd.format(**all_params)
        except KeyError as e:
            print(f"Missing parameter for {e}")

    if interactive:
        curses.endwin()
        subprocess.run(cmd, shell=True)
        curses.doupdate()
        return

    if log_win is None:
        subprocess.run(cmd, shell=True)
        return

    log_win.erase()
    log_win.box()
    log_win.addstr(1, 2, f"Running: {cmd}")
    log_win.refresh()

    process = subprocess.Popen(
        cmd, shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    h, w = log_win.getmaxyx()
    row = 2
    for line in process.stdout:
        try:
            log_win.addstr(row, 2, line.rstrip()[:w - 4])
        except curses.error:
            pass
        row += 1
        if row >= h - 2:
            log_win.erase()
            log_win.box()
            log_win.addstr(1, 2, f"Running: {cmd}")
            row = 2
        log_win.refresh()

    process.wait()
    try:
        log_win.addstr(h - 2, 2, "Done. Press any key...")
    except curses.error:
        pass
    log_win.refresh()
    log_win.getch()


# ----------- UI Helpers -----------

def wrap_text(text, width):
    words, lines, line = text.split(), [], ""
    for word in words:
        while len(word) > width:
            if line:
                lines.append(line)
                line = ""
            lines.append(word[:width])
            word = word[width:]
        if len(line) + len(word) + 1 <= width:
            line = f"{line} {word}".strip()
        else:
            if line:
                lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


class UI:
    def __init__(self, stdscr, menu, config):
        self.stdscr = stdscr
        self.menu = menu
        self.cfg = config
        self.current = 0
        self.last_resize = 0
        self.create_windows()

    def create_windows(self):
        h, w = self.stdscr.getmaxyx()
        menu_w = w // 2
        hint_w = w - menu_w
        log_h = h // 3
        self.menu_win = curses.newwin(h - log_h, menu_w, 0, 0)
        self.hint_win = curses.newwin(h - log_h, hint_w, 0, menu_w)
        self.log_win = curses.newwin(log_h, w, h - log_h, 0)
        self.log_win.box()
        self.log_win.refresh()

    def draw(self):
        self.menu_win.erase()
        self.hint_win.erase()
        self.menu_win.box()
        self.hint_win.box()
        self.menu_win.addstr(0, 2, " MENU ")
        self.hint_win.addstr(0, 2, " DESCRIPTION ")

        mh, mw = self.menu_win.getmaxyx()
        hh, hw = self.hint_win.getmaxyx()
        max_visible = mh - 4
        max_name = mw - 6

        item_lines = [wrap_text(name, max_name)
                      for name, *_ in self.menu.items]
        offset = max(0, self.current - max_visible + 1)
        row = 0
        for i, lines in enumerate(item_lines):
            if i < offset:
                continue
            if row >= max_visible:
                break
            for j, line in enumerate(lines):
                if row >= max_visible:
                    break
                prefix = "> " if i == self.current and j == 0 else "  "
                attr = curses.A_BOLD if i == self.current else curses.A_NORMAL
                self.menu_win.addstr(2 + row, 2, f"{prefix}{line}", attr)
                row += 1

        if len(self.menu.items) > max_visible:
            self.menu_win.addstr(
                mh - 1, 2, f" {self.current + 1}/{len(self.menu.items)} ")

        name, cmd, args, desc, interactive = self.menu.items[self.current]
        for i, line in enumerate(wrap_text(desc, hw - 4)):
            if 1 + i >= hh - 1:
                break
            self.hint_win.addstr(1 + i, 2, line)

        self.menu_win.refresh()
        self.hint_win.refresh()

    def handle_resize(self):
        now = time.time()
        if now - self.last_resize > 0.1:
            self.last_resize = now
            curses.resizeterm(*self.stdscr.getmaxyx())
            self.create_windows()


# ----------- CLI Runner -----------

def run_cli_command(menu, cfg, name):
    item = menu.get_by_name(name)
    if not item:
        print(f"Command {name} not found")
        return
    # Распаковываем кортеж: name, cmd, args, desc, interactive
    name, cmd, menu_args, _, interactive = item
    params = cfg.get_params(name)  # значения из TOML
    run_command(cmd, log_win=None, interactive=interactive,
                cfg_params=params, menu_args=menu_args)

# ----------- Main -----------


def main(stdscr):
    curses.curs_set(0)
    stdscr.nodelay(False)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, -1, -1)
    curses.init_pair(2, curses.COLOR_BLACK, -1)
    stdscr.bkgd(' ', curses.color_pair(1))

    cfg = Config()
    menu = Menu()
    ui = UI(stdscr, menu, cfg)

    # CLI direct command
    if len(sys.argv) > 1:
        run_cli_command(menu, cfg, sys.argv[1])
        return

    while True:
        ui.draw()
        key = stdscr.getch()
        if key == curses.KEY_RESIZE:
            ui.handle_resize()
            continue
        elif key == curses.KEY_UP:
            ui.current = (ui.current - 1) % len(menu.items)
        elif key == curses.KEY_DOWN:
            ui.current = (ui.current + 1) % len(menu.items)
        elif key in [10, 13]:
            name, cmd, _, interactive = menu.items[ui.current]
            if cmd is None:
                break
            params = cfg.get_params(name)
            run_command(cmd, ui.log_win, interactive, cfg_params=params)
            # refresh windows after command
            ui.create_windows()


if __name__ == "__main__":
    curses.wrapper(main)
