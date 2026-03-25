import time
import curses
import subprocess
import json
import sys


def load_menu(path):
    with open(path) as f:
        data = json.load(f)
    return [(
        item["name"],
        item["cmd"],
        item["desc"],
        item.get("interactive", False)
    ) for item in data]


def run_command(cmd, log_win, interactive=False):
    if interactive:
        # Exit curses, run, return
        curses.endwin()
        subprocess.run(cmd, shell=True)
        curses.doupdate()
        return

    log_win.erase()
    log_win.box()
    log_win.addstr(1, 2, f"Running: {cmd}")
    log_win.refresh()

    process = subprocess.Popen(
        cmd,
        shell=True,
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


def wrap_text(text, width):
    words = text.split()
    lines = []
    line = ""
    for word in words:
        # If the word itself is longer than width, we cut it
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


def draw_menu(menu_win, hint_win, current, menu):
    menu_win.erase()
    hint_win.erase()
    menu_win.box()
    hint_win.box()
    menu_win.addstr(0, 2, " MENU ")
    hint_win.addstr(0, 2, " DESCRIPTION ")

    mh, mw = menu_win.getmaxyx()
    hh, hw = hint_win.getmaxyx()
    max_visible = mh - 4
    max_name = mw - 6

    item_lines = []
    for name, _, _, _ in menu:
        item_lines.append(wrap_text(name, max_name))

    offset = max(0, current - max_visible + 1)

    row = 0
    for i, lines in enumerate(item_lines):
        if i < offset:
            continue
        if row >= max_visible:
            break

        for j, line in enumerate(lines):
            if row >= max_visible:
                break
            y = 2 + row
            prefix = "> " if (i == current and j == 0) else "  "
            attr = curses.A_BOLD if i == current else curses.A_NORMAL
            menu_win.addstr(y, 2, f"{prefix}{line}", attr)
            row += 1

    if len(menu) > max_visible:
        menu_win.addstr(mh - 1, 2, f" {current + 1}/{len(menu)} ")

    _, _, desc, _ = menu[current]
    for i, line in enumerate(wrap_text(desc, hw - 4)):
        if 1 + i >= hh - 1:
            break
        hint_win.addstr(1 + i, 2, line)

    menu_win.refresh()
    hint_win.refresh()


def main(stdscr):
    menu_path = sys.argv[1] if len(sys.argv) > 1 else "menu.json"
    menu = load_menu(menu_path)

    curses.curs_set(0)
    stdscr.nodelay(False)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, -1, -1)
    curses.init_pair(2, curses.COLOR_BLACK, -1)
    stdscr.bkgd(' ', curses.color_pair(1))

    def create_windows():
        stdscr.clear()
        stdscr.refresh()
        h, w = stdscr.getmaxyx()
        menu_w = w // 2
        hint_w = w - menu_w
        log_h = h // 3
        menu_win = curses.newwin(h - log_h, menu_w, 0, 0)
        hint_win = curses.newwin(h - log_h, hint_w, 0, menu_w)
        log_win = curses.newwin(log_h, w, h - log_h, 0)
        log_win.box()
        log_win.refresh()
        return menu_win, hint_win, log_win

    menu_win, hint_win, log_win = create_windows()
    current = 0
    last_resize = 0

    while True:
        draw_menu(menu_win, hint_win, current, menu)
        key = stdscr.getch()

        if key == curses.KEY_RESIZE:
            now = time.time()
            if now - last_resize > 0.1:  # ignore if resize was recently
                last_resize = now
                curses.resizeterm(*stdscr.getmaxyx())
                menu_win, hint_win, log_win = create_windows()
            continue

        if key == curses.KEY_UP:
            current = (current - 1) % len(menu)
        elif key == curses.KEY_DOWN:
            current = (current + 1) % len(menu)
        elif key in [10, 13]:
            name, cmd, _, interactive = menu[current]
            if cmd is None:
                break
            run_command(cmd, log_win, interactive)

            stdscr.touchwin()
            stdscr.refresh()
            menu_win.touchwin()
            menu_win.refresh()
            hint_win.touchwin()
            hint_win.refresh()
            log_win.touchwin()
            log_win.refresh()


if __name__ == "__main__":
    curses.wrapper(main)
