import curses


def main(stdscr):
    # Clear screen
    stdscr.clear()

    # Print a message at row 5, column 10
    stdscr.addstr(5, 10, "Hello, Curses!")
    stdscr.refresh()

    # Wait for any key press before exiting
    stdscr.getch()


# The wrapper handles setup and cleanup automatically
curses.wrapper(main)
