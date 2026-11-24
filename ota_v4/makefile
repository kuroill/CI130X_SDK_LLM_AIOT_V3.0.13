PWD 	:= `pwd`
CC 		= gcc
#CC 		=  /opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc 
SRC_PATH 	:= .
CFLAGS 		= -Os -fPIC -w   -g  -Wall -ggdb -lm -lpthread  -std=gnu99
OTA_OBJS 	= $(wildcard $(SRC_PATH)/*.c) 
targets = spilt_firmware

all:$(targets)
$(targets):$(OTA_OBJS)
	$(CC)  $(OTA_OBJS)  $(CFLAGS)  -o $@ 

clean:
	rm -f *.o $(targets)

