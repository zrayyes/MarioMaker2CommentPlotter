import pygame


COLORS = [
    (1, "fe1600", "Light red"),  # Light red
    (2, "bb1000", "Dark red"),  # Dark red
    (3, "fff3d0", "Light brown"),  # Light brown
    (4, "ad7f46", "Dark brown"),  # Dark brown
    (5, "fff001", "Yellow"),  # Yellow
    (6, "fec100", "Orange"),  # Orange
    (7, "03d901", "Light green"),  # Light green
    (8, "00bb00", "Dark green"),  # Dark green
    (9, "01e9ff", "Light blue"),  # Light blue
    (10, "000eff", "Dark blue"),  # Dark blue
    (11, "ba62fe", "Light purple"),  # Light purple
    (12, "8617ba", "Dark purple"),  # Dark purple
    (13, "fec0fd", "Light pink"),  # Light pink
    (14, "b81889", "Dark pink"),  # Dark pink
    (15, "bbbabb", "Gray"),  # Gray
    (16, "000000", "Black"),  # Black
    (17, "ffffff", "White"),  # White
]

RTRIGGER = 7
LTRIGGER = 6

# Define some colors.
BLACK = pygame.Color('black')
WHITE = pygame.Color('white')


# This is a simple class that will help us print to the screen.
# It has nothing to do with the joysticks, just outputting the
# information.
class TextPrint(object):
    def __init__(self):
        self.reset()
        self.font = pygame.font.Font(None, 20)

    def tprint(self, screen, textString):
        textBitmap = self.font.render(textString, True, BLACK)
        screen.blit(textBitmap, (self.x, self.y))
        self.y += self.line_height

    def reset(self):
        self.x = 10
        self.y = 10
        self.line_height = 15

    def indent(self):
        self.x += 10

    def unindent(self):
        self.x -= 10


def change_color(color_counter, direction):
    if color_counter == 17 and direction == "R":
        color_counter = 1
    elif color_counter == 1 and direction == "L":
        color_counter = 17
    else:
        color_counter += 1 if direction == "R" else -1
    return color_counter


pygame.init()

# Set the width and height of the screen (width, height).
screen = pygame.display.set_mode((500, 700))

pygame.display.set_caption("My Game")

# Loop until the user clicks the close button.
done = False

# Used to manage how fast the screen updates.
clock = pygame.time.Clock()

# Initialize the joysticks.
pygame.joystick.init()

# Get ready to print.
textPrint = TextPrint()


# To Display
color_counter = 16
current_hat = "CENTER"

# -------- Main Program Loop -----------
while not done:
    #
    # EVENT PROCESSING STEP
    #
    # Possible joystick actions: JOYAXISMOTION, JOYBALLMOTION, JOYBUTTONDOWN,
    # JOYBUTTONUP, JOYHATMOTION
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            done = True
        elif event.type == pygame.JOYBUTTONDOWN:
            if event.button in (RTRIGGER, LTRIGGER):
                color_counter = change_color(
                    color_counter, "L" if event.button == LTRIGGER else "R")
                print("Color changed to", COLORS[color_counter-1][2])
            if event.button == 2:
                print("PAINT", COLORS[color_counter-1][2])
        elif event.type == pygame.JOYHATMOTION:
            x = event.value[0]
            y = event.value[1]
            if x == 0 and y == 0:
                current_hat = "CENTER"
            elif x == 1 and y == 0:
                current_hat = "RIGHT"
            elif x == -1 and y == 0:
                current_hat = "LEFT"
            elif x == 0 and y == -1:
                current_hat = "DOWN"
            elif x == 0 and y == 1:
                current_hat = "UP"
            if current_hat != "CENTER":
                print(current_hat)

    #
    # DRAWING STEP
    #
    # First, clear the screen to white. Don't put other drawing commands
    # above this, or they will be erased with this command.
    screen.fill(WHITE)
    textPrint.reset()

    joystick = pygame.joystick.Joystick(0)
    joystick.init()

    textPrint.tprint(screen, "Current Color: {}".format(
        COLORS[color_counter-1][2]))

    textPrint.tprint(screen, "Current HAT: {}".format(current_hat))

    #
    # ALL CODE TO DRAW SHOULD GO ABOVE THIS COMMENT
    #

    # Go ahead and update the screen with what we've drawn.
    pygame.display.flip()

    # Limit to 20 frames per second.
    clock.tick(20)

# Close the window and quit.
# If you forget this line, the program will 'hang'
# on exit if running from IDLE.
pygame.quit()
