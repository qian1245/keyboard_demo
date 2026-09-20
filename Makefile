CC = gcc
CFLAGS = -I. -I./lvgl -O3 -Wall
LDFLAGS = -lSDL2 -lm

# 自动搜寻 src 下的 C 文件以及 lvgl 核心源文件
SRCS = src/main.c src/keyboard.c assets/bubble_dot.c $(shell find lvgl/src -name "*.c")
OBJS = $(SRCS:.c=.o)

TARGET = build/keyboard_app

$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(OBJS)

.PHONY: clean