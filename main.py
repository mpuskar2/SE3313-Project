from socket import AF_INET, socket, SOCK_STREAM
from threading import Thread
import pygame

# Global variables
close = False
isP1 = False
isP2 = False
run = True
otherPMove = ""
sendPMove = ""
ballx = 0
bally = 0
p1score = 0
p2score = 0

# Receive data from the server
def receive():
    while True:
        global isP1, isP2
        try:
            data = client_socket.recv(BUFFER_SIZE).decode("utf8")  # Decode data from other client
            arr = data.split(',')
            if arr[0] == "p1": # If p1 is received, this client will be p1
                isP1 = True
            elif arr[0] == "p2": # If p2 is received, this client will be p2
                isP2 = True
            global otherPMove
            otherPMove = arr[0]
            if isP2 and arr[0] != "p2":
                global ballx, bally, p1score, p2score
                ballx = arr[1]
                bally = arr[2]
                p1score = arr[3]
                p2score = arr[4]
        except OSError:
            break

# Send data to server
def send(event=None):
    try:
        global ballx, bally, p1score, p2score
        data = sendPMove
        if isP1:
            data += f",{ballx},{bally},{p1score},{p2score}"
        data += ','
        if close:  # Check if the user decides to quit
            client_socket.send(bytes("shutdown", "utf8"))
            client_socket.close()  # Close client thread on server
            return
        client_socket.send(bytes(data, "utf8"))
    except:
        print("No response from server, closing game")
        global run
        run = False

# Send quit message to the server
def on_closing(event=None):
    global close
    close = True
    send()

# Socket with server parameters
HOST = "127.0.0.1"
PORT = 3005
BUFFER_SIZE = 1024
ADDR = (HOST, PORT)

# Create connection to server
client_socket = socket(AF_INET, SOCK_STREAM)
client_socket.connect(ADDR)

# Get welcome message from the server
first_msg = client_socket.recv(BUFFER_SIZE).decode("utf8")
print(first_msg)

# Create thread for receiving data from the server
receive_thread = Thread(target=receive)
receive_thread.start()

### GAME CODE ###

# Initialize pygame objects and constants
pygame.init()
FPS = 60
WIDTH, HEIGHT = 700, 500
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
PADDLE_WIDTH, PADDLE_HEIGHT = 20, 100
BALL_RADIUS = 7
WINNING_SCORE = 5

WINDOW = pygame.display.set_mode((WIDTH, HEIGHT))
SCORE_FONT = pygame.font.SysFont("Arial", 50)
pygame.display.set_caption('Pong')

class Paddle:
    COLOR = WHITE
    VELOCITY = 4

    def __init__(self, x, y, width, height):
        self.x = self.original_x = x
        self.y = self.original_y = y
        self.width = width
        self.height = height

    # Display paddle on screen
    def draw(self, window):
        pygame.draw.rect(window, self.COLOR, (self.x, self.y, self.width, self.height))

    # Change direction of paddle movement
    def move(self, up=True):
        if up:
            self.y -= self.VELOCITY
        else:
            self.y += self.VELOCITY

    # Reset location of paddle
    def reset(self):
        self.x = self.original_x
        self.y = self.original_y

class Ball:
    MAX_VELOCITY = 5
    COLOR = WHITE

    def __init__(self, x, y, radius):
        self.x = self.original_x = x
        self.y = self.original_y = y
        self.radius = radius
        self.x_velocity = self.MAX_VELOCITY
        self.y_velocity = 0

    # Display ball on screen
    def draw(self, window):
        pygame.draw.circle(window, self.COLOR, (self.x, self.y), self.radius)

    # Change direction of ball movement
    def move(self):
        self.x += self.x_velocity
        self.y += self.y_velocity

    # Reset location of ball
    def reset(self):
        self.x = self.original_x
        self.y = self.original_y
        self.y_velocity = 0
        self.x_velocity *= -1

# Draw window, paddles ball and scores on screen
def draw(window, paddles, ball, score1, score2):
    window.fill(BLACK)

    # Insert score text
    score1_score_text = SCORE_FONT.render(f"{score1}", 1, WHITE)
    score2_score_text = SCORE_FONT.render(f"{score2}", 1, WHITE)
    window.blit(score1_score_text, (WIDTH//4 - score1_score_text.get_width()//2, 20))
    window.blit(score2_score_text, (WIDTH * (3 / 4) - score2_score_text.get_width() // 2, 20))

    # Draw both paddles
    for p in paddles:
        p.draw(window)

    # Draw center line
    for i in range(10, HEIGHT, HEIGHT//20):
        if i % 2 == 1:
            continue
        pygame.draw.rect(window, WHITE, (WIDTH//2 - 5, i, 10, HEIGHT//20))

    # Draw ball and update screen
    ball.draw(window)
    pygame.display.update()

# Handle collision with ball and paddles
def handle_collision(ball, p1, p2):
    # If ball hit top or bottom border
    if ball.y + ball.radius >= HEIGHT:
        ball.y_velocity *= -1
    elif ball.y - ball.radius <= 0:
        ball.y_velocity *= -1

    # If ball hit left paddle
    if ball.x_velocity < 0:
        if ball.y >= p1.y and ball.y <= p1.y + p1.height:
            if ball.x - ball.radius <= p1.x + p1.width:
                ball.x_velocity *= -1

                middle_y = p1.y + p1.height / 2
                difference_in_y = middle_y - ball.y
                reduction_factor = (p1.height / 2) / ball.MAX_VELOCITY
                y_velocity = difference_in_y / reduction_factor
                ball.y_velocity = -1 * y_velocity

    # If ball hit right paddle
    else:
        if ball.y >= p2.y and ball.y <= p2.y + p2.height:
            if ball.x + ball.radius >= p2.x:
                ball.x_velocity *= -1

                middle_y = p2.y + p2.height / 2
                difference_in_y = middle_y - ball.y
                reduction_factor = (p2.height / 2) / ball.MAX_VELOCITY
                y_velocity = difference_in_y / reduction_factor
                ball.y_velocity = -1 * y_velocity
    
    global ballx, bally
    ballx = ball.x
    bally = ball.y

# Handle paddle movement
def handle_paddle_movement(keys, p1, p2):
    global sendPMove

    # For left paddle
    if isP1:
        # W to move up and S to move down
        if keys[pygame.K_w] and p1.y - p1.VELOCITY >= 0:
            p1.move(up=True)
            sendPMove = "up"
            send()
        elif keys[pygame.K_s] and p1.y + p1.VELOCITY + p1.height <= HEIGHT:
            p1.move(up=False)
            sendPMove = "down"
            send()
        else:
            sendPMove = "n"
            send()

        # Update movement of opposite player paddle
        if otherPMove == "up" and p2.y - p2.VELOCITY >= 0:
            p2.move(up=True)
        elif otherPMove == "down" and p2.y + p2.VELOCITY + p2.height <= HEIGHT:
            p2.move(up=False)

    # For right paddle
    if isP2:
        # Update movement of opposite player paddle
        if otherPMove == "up" and p1.y - p1.VELOCITY >= 0:
            p1.move(up=True)
        elif otherPMove == "down" and p1.y + p1.VELOCITY + p1.height <= HEIGHT:
            p1.move(up=False)

        # W to move up and S to move down
        if keys[pygame.K_w] and p2.y - p2.VELOCITY >= 0:
            p2.move(up=True)
            sendPMove = "up"
            send()
        elif keys[pygame.K_s] and p2.y + p2.VELOCITY + p2.height <= HEIGHT:
            p2.move(up=False)
            sendPMove = "down"
            send()
        else:
            sendPMove = "n"
            send()

def main():
    global run, isP1, ballx, bally, p2score, p1score
    # Create paddles, ball and clock
    clock = pygame.time.Clock()
    p1 = Paddle(10, HEIGHT // 2 - PADDLE_HEIGHT // 2, PADDLE_WIDTH, PADDLE_HEIGHT)
    p2 = Paddle(WIDTH - (PADDLE_WIDTH + 10), HEIGHT // 2 - PADDLE_HEIGHT // 2, PADDLE_WIDTH, PADDLE_HEIGHT)
    ball = Ball(WIDTH // 2, HEIGHT // 2, BALL_RADIUS)

    # Set starting scores
    p1_score = 0
    p2_score = 0

    while run:
        # Set frame rate and open up window
        clock.tick(FPS)
        draw(WINDOW, [p1, p2], ball, p1_score, p2_score)
        
        # Quit app when window is closed
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                run = False
                break

        # Update movement of ball and paddles
        keys = pygame.key.get_pressed()
        handle_paddle_movement(keys, p1, p2)

        if isP1:
            ball.move()
            handle_collision(ball, p1, p2)
            # Update score when player scores
            if ball.x < 0:
                p2_score += 1
                p2score = p2_score
                ball.reset()
            elif ball.x > WIDTH:
                p1_score += 1
                p1score = p1_score
                ball.reset()
        else:
            ball.x = float(ballx)
            ball.y = float(bally)
            p2_score = int(p2score)
            p1_score = int(p1score)



        # Check if someone has won
        won = False
        if p1_score >= WINNING_SCORE:
            won = True
            win_text = "Player 1 Won!"
        elif p2_score >= WINNING_SCORE:
            won = True
            win_text = "Player 2 Won!"

        # Display if someone has won
        if won:
            text = SCORE_FONT.render(win_text, 1, WHITE)
            WINDOW.blit(text, (WIDTH // 2 - text.get_width() // 2, HEIGHT // 2 - text.get_height() // 2))
            pygame.display.update()
            pygame.time.delay(5000)
            ball.reset()
            p1.reset()
            p2.reset()
            p1_score = 0
            p2_score = 0
            p1score = 0
            p2score = 0

    on_closing()
    pygame.quit()

if __name__ == '__main__':
    main()