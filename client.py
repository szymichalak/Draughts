from tkinter import *
import tkinter as tk
import socket
import threading
import sys
import time

HEIGHT = 800
WIDTH = 950
s = socket.socket()
connected = False
color = ""
rows = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H']
columns = ['1', '2', '3', '4', '5', '6', '7', '8']


def connect_to_server(ip_addr, port_no):
    global connected
    global s
    if not connected:
        try:
            port_no = int(port_no)
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((ip_addr, port_no))
            show_information(display, "Connected! Good luck!")
            connected = True
        except (ConnectionRefusedError, ValueError):
            show_information(display, "Something went wrong. Check the address of server and port's number")
    else:
        show_information(display, "You have already connected")


def disconnect(socket_des):
    global connected
    if connected:
        connected = False
        socket_des.close()
        time.sleep(1)
        sys.exit()
    else:
        show_information(display, "At first you have to connect and then you are able to disconnect")


def send_data(socket_des, field_one, field_two):
    if len(field_one) == 2 and len(field_two) == 2:
        message = str(field_one) + ";" + str(field_two) + "\n"
        message_byte = bytes(message, 'utf-8')
        socket_des.send(message_byte)
    else:
        show_information(display, "Check required fields")


def receive_data(socket_des):
    buffer_size = 1024
    read = 0
    data_final = ""
    while read < 6:
        data = socket_des.recv(buffer_size)
        data = str(data, 'utf-8')
        if data.find('.') != -1:
            return [("er", "03")]
        read += len(data)
        data_final += data
    if len(data_final) == 6:
        field_one = data_final[:2]
        field_two = data_final[3:5]
        return [(field_one, field_two)]
    elif len(data_final) % 6 == 0:
        final_list = []
        for _ in range(int(len(data_final) / 6)):
            final_list.append((data_final[:2], data_final[3:5]))
            data_final = data_final[6:]
        return final_list
    else:
        return [("er", "01")]  # bad receive code


def receive_color(socket_des, thread):
    global color
    global connected
    if connected:
        if color == "":
            buffer_size = 5
            while len(color) < 5:
                data = socket_des.recv(buffer_size)
                color_string = str(data, 'utf-8')
                color = color + color_string
            if color == "white":
                show_information(display, "You are " + color + ". Start!")
            elif color == "black":
                show_information(display, "You are " + color + ". Wait for white move")
            thread.start()
        else:
            show_information(display, "You are " + color)
    else:
        show_information(display, "Connect at first, then you will receive your color")


def read_and_update():
    global s
    global connected
    global color

    if color == "white":
        motion_button.config(state="normal")

    while connected:
        receive_list = receive_data(s)
        while receive_list:
            x = receive_list[0][0]
            y = receive_list[0][1]
            if "A" <= x[0] <= "H" and "A" <= y[0] <= "H" and "1" <= x[1] <= "8" and "1" <= y[1] <= "8":
                row1 = rows.index(x[0])
                col1 = columns.index(x[1])
                row2 = rows.index((y[0]))
                col2 = columns.index((y[1]))
                id_man = game[row1][col1]

                if abs(row1 - row2) == 2:
                    captured = game[int((row1 + row2) / 2)][int((col1 + col2) / 2)]
                    if 1 <= captured <= 12:
                        blacks[captured - 1].after(0, blacks[captured - 1].destroy())
                    elif 13 <= captured <= 24:
                        whites[captured - 13].after(0, whites[captured - 13].destroy())
                    game[int((row1 + row2) / 2)][int((col1 + col2) / 2)] = 0

                if 1 <= id_man <= 12:
                    blacks[id_man - 1].after(0, blacks[id_man - 1].destroy())
                    blacks[id_man - 1] = Label(field[row2][col2], bg='#000000', image=black_man)
                    blacks[id_man - 1].place(relwidth=1, relheight=1)
                    game[row2][col2] = id_man
                    game[row1][col1] = 0
                elif 13 <= id_man <= 24:
                    whites[id_man - 13].after(0, whites[id_man - 13].destroy())
                    whites[id_man - 13] = Label(field[row2][col2], bg='#000000', image=white_man)
                    whites[id_man - 13].place(relwidth=1, relheight=1)
                    game[row2][col2] = id_man
                    game[row1][col1] = 0

                if start.get() == x and finish.get() == y:
                    motion_button.config(state="disable")
                    show_information(display, "Wait...")
                else:
                    motion_button.config(state="normal")
                    show_information(display, "Now it's your turn")

            elif x == "er" and y == "00":
                show_information(display, "Bad data, check required fields one more time")

            elif x == "er" and y == "01":
                show_information(display, "Receive error, you have to disconnect or close window")
                # connected = False
                connection_button.config(state="disable")
                # disconnection_button.config(state="disable")
                motion_button.config(state="disable")
                set_color.config(state="disable")

            elif x == "er" and t == "02":
                show_information(display, "An unexpected problem occurred, server is not able to read your request")
                connection_button.config(state="disable")
                disconnection_button.config(state="disable")
                motion_button.config(state="disable")
                set_color.config(state="disable")

            elif x == "er" and t == "03":
                show_information(display, "Server can't send data correctly")
                connection_button.config(state="disable")
                disconnection_button.config(state="disable")
                motion_button.config(state="disable")
                set_color.config(state="disable")

            elif x == "ov" and y == "er":
                show_information(display, "Your opponent lost connection. You are a winner! Congratulations!")
                connection_button.config(state="disable")
                disconnection_button.config(state="disable")
                motion_button.config(state="disable")
                set_color.config(state="disable")
                show_information(display, "I hope you enjoy this game ;)")
                connected = False

            elif x == "wh" and y == "bl":
                show_information(display, "White is a winner! Congratulation! Black lost")
                show_information(display, "I hope you enjoy this game ;)")
                connection_button.config(state="disable")
                disconnection_button.config(state="disable")
                motion_button.config(state="disable")
                set_color.config(state="disable")
                connected = False

            elif x == "bl" and y == "wh":
                show_information(display, "Black is a winner! Congratulation! White lost")
                show_information(display, "I hope you enjoy this game ;)")
                connection_button.config(state="disable")
                disconnection_button.config(state="disable")
                motion_button.config(state="disable")
                set_color.config(state="disable")
                connected = False

            else:
                show_information(display, "i don't even know what happened....")

            del receive_list[0]


def show_information(active_display, info):
    read_text = active_display.cget("text")
    new_lines = read_text.count("\n")
    if new_lines < 6:
        new_string = read_text + info + "\n"
        display.configure(text=new_string)
    else:
        first_nl = read_text.index("\n")
        new_string = read_text[first_nl + 1:] + info + "\n"
        display.configure(text=new_string)


# start of the drawing section
root = tk.Tk()

# set resolution of window
canvas = tk.Canvas(root, height=HEIGHT, width=WIDTH)
canvas.pack()

# create a thread which waits for data from server
t = threading.Thread(target=read_and_update)
t.daemon = True

# draw board and labels, field matrix contains images (man or nothing)
board_labels = tk.Frame(root)
board_labels.place(relx=0.05, rely=0.15, relwidth=0.65, relheight=0.65)
board = tk.Frame(board_labels, bg='#000000')
board.place(relx=0.05, rely=0.05, relwidth=0.9, relheight=0.9)
for i in range(8):
    label_row1 = tk.Label(board_labels, text=rows[i])
    label_row1.place(relx=0.02, rely=0.1 + 0.9 / 8 * i)
    label_row2 = tk.Label(board_labels, text=rows[i])
    label_row2.place(relx=0.96, rely=0.1 + 0.9 / 8 * i)
    label_col1 = tk.Label(board_labels, text=columns[i])
    label_col1.place(relx=0.1 + 0.9 / 8 * i, rely=0)
    label_col2 = tk.Label(board_labels, text=columns[i])
    label_col2.place(relx=0.1 + 0.9 / 8 * i, rely=0.96)
field = []
for k in range(8):
    field.append([tk.Frame() for f in range(8)])
for i in range(8):
    for j in range(8):
        if (i + j) % 2 == 0:
            field[i][j] = tk.Frame(board, bg='#000000')
            field[i][j].place(relx=j / 8, rely=i / 8, relwidth=0.125, relheight=0.125)
        else:
            field[i][j] = tk.Frame(board, bg='#FFFFFF')
            field[i][j].place(relx=j / 8, rely=i / 8, relwidth=0.125, relheight=0.125)

# create connection box, set ip, port, connect and disconnect
connection_box = tk.Frame(root, bg='#000000')
connection_box.place(relx=0.725, rely=0.15, relwidth=0.25, relheight=0.25)
ip = tk.Entry(connection_box, font=20)
ip.place(relx=0.45, rely=0.1, relwidth=0.5, relheight=0.15)
port = tk.Entry(connection_box, font=20)
port.place(relx=0.45, rely=0.4, relwidth=0.5, relheight=0.15)
label_connection1 = tk.Label(connection_box, text="Server's IP")
label_connection1.place(relx=0.05, rely=0.1, relwidth=0.35, relheight=0.15)
label_connection2 = tk.Label(connection_box, text="Port")
label_connection2.place(relx=0.05, rely=0.4, relwidth=0.35, relheight=0.15)
connection_button = tk.Button(connection_box, text="Connect", command=lambda: connect_to_server(ip.get(), port.get()))
connection_button.place(relx=0.05, rely=0.7, relwidth=0.4, relheight=0.15)
disconnection_button = tk.Button(connection_box, text="Disconnect", command=lambda: disconnect(s))
disconnection_button.place(relx=0.55, rely=0.7, relwidth=0.4, relheight=0.15)

# create motion box, move from one field to another
motion_box = tk.Frame(root, bg='#007000')
motion_box.place(relx=0.725, rely=0.5, relwidth=0.25, relheight=0.25)
start = tk.Entry(motion_box, font=20)
start.place(relx=0.45, rely=0.1, relwidth=0.5, relheight=0.15)
finish = tk.Entry(motion_box, font=20)
finish.place(relx=0.45, rely=0.4, relwidth=0.5, relheight=0.15)
label_motion1 = tk.Label(motion_box, text="Move from")
label_motion1.place(relx=0.05, rely=0.1, relwidth=0.35, relheight=0.15)
label_motion2 = tk.Label(motion_box, text="Move to")
label_motion2.place(relx=0.05, rely=0.4, relwidth=0.35, relheight=0.15)
motion_button = tk.Button(motion_box, text="Move", state=DISABLED,
                          command=lambda: send_data(s, start.get(), finish.get()))
motion_button.place(relx=0.25, rely=0.7, relwidth=0.5, relheight=0.15)

# button to set or check your color
set_color = tk.Button(root, text="Set your color!", command=lambda: receive_color(s, t))
set_color.place(relx=0.775, rely=0.425, relwidth=0.15, relheight=0.05)

# heading label
main_label = tk.Label(root, text="Draughts", font=40)
main_label.place(relx=0.1, rely=0, relwidth=0.8, relheight=0.15)

# console where you can read notifications
display = Label(root, text="", bg='#000000', fg='#FFFFFF')
display.place(relx=0, rely=0.8, relwidth=1, relheight=0.2)

# photo import
black_man = PhotoImage(file="black.png")
white_man = PhotoImage(file="white.png")

# game matrix contains id/0 for every field
game = []
for p in range(8):
    game.append([0, 0, 0, 0, 0, 0, 0, 0])

# initial place men and set them id (blacks <1, 12>, whites <13, 24>), add id to game matrix
blacks = []
whites = []
index = 1
for k in range(8):
    for j in range(8):
        if (k + j) % 2 == 0 and k <= 2:
            blacks.append(Label(field[k][j], bg='#000000', image=black_man))
            game[k][j] = index
            index += 1
        if (k + j) % 2 == 0 and k >= 5:
            whites.append(Label(field[k][j], bg='#000000', image=white_man))
            game[k][j] = index
            index += 1
for man in blacks:
    man.place(relwidth=1, relheight=1)
for man in whites:
    man.place(relwidth=1, relheight=1)

# end of the drawing section
root.mainloop()
