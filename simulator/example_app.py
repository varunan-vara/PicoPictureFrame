"""Example app: the same sort of drawing code you'd write against canvas.h, at 160x120."""

from picodisplay import Display, rgb565


def main():
    display = Display(width=160, title="Example app (160x120)")

    while display.poll():
        display.fill_rect(0, 0, display.w, display.h, rgb565(20, 20, 30))

        display.text(8, 8, "PAPA PIZZA", 1, rgb565(255, 200, 100))
        display.fill_rect(8, 18, 144, 1, rgb565(80, 80, 100))

        display.text(8, 26, "SCORE 02400", 1, rgb565(255, 255, 255))
        display.text(8, 36, "TIME 01:45", 1, rgb565(200, 100, 100))

        # Health bar
        display.fill_rect(8, 48, 144, 16, rgb565(50, 50, 70))
        display.fill_rect(10, 50, 100, 12, rgb565(100, 220, 100))
        display.text(13, 52, "HP", 1, rgb565(255, 255, 255))

        display.text(8, 100, "PRESS A TO BAKE", 1, rgb565(100, 200, 255))

        # Like the real screen: 1px border to check nothing is cropped
        display.rect_outline(0, 0, display.w, display.h, rgb565(255, 0, 0))

        display.show()

    display.close()


if __name__ == "__main__":
    main()
