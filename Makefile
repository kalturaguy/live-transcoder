CC = gcc -v
SRC = $(wildcard *.c)
DEPS = $(wildcard *.h)
ODIR=obj
_OBJ = $(SRC:.c=.o)
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
_EXE = transcoder
EXE = $(patsubst %,$(ODIR)/%,$(_EXE))
ifeq ($(OS), Linux)
        FFMPEG_LIB_DIR := ./ffmpeg
        X264_LIB_DIR := ./x264
else
        FFMPEG_LIB_DIR := ./ThirdParty/ffmpeg
        X264_LIB_DIR := ./ThirdParty/x264
endif

IDIRS = -I. -I$(FFMPEG_LIB_DIR)
LDIR = -L/usr/local/cuda/lib64 -L$(X264_LIB_DIR) -L$(FFMPEG_LIB_DIR)/libswscale -L$(FFMPEG_LIB_DIR)/libavdevice -L$(FFMPEG_LIB_DIR)/libavutil -L$(FFMPEG_LIB_DIR)/libavformat -L$(FFMPEG_LIB_DIR)/libavcodec -L$(FFMPEG_LIB_DIR)/libpostproc -L/usr/local/lib
FFMPEG_LIBS = -lavfilter  -lavformat -lswscale  -lavcodec   -lavutil  -lswresample -lpostproc -lx264  
LIBS = -lm -lpthread  -lz -lbz2 -ldl -lrt   
CUDA_LIBS = -lnppig_static -lnppicc_static -lnppc_static -lnppidei_static -lcublas_static -lcudart_static -lculibos  -lcudart -lstdc++
CFLAGS = -Wall -g $(IDIRS) -fPIC 
LDFLAGS =  $(LDIR) $(LIBS) ${CUDA_LIBS} $(FFMPEG_LIBS) ${CUDA_LIBS} $(LIBS) 
OS := $(shell uname)

ifeq ($(OS), Linux)
        LIBS += -lrt
else
        LIBS += -liconv
endif

dir_guard=@mkdir -p $(@D)

$(ODIR)/%.o: %.c $(DEPS)        
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(phony install): install

install: $(EXE)
	mkdir -p ../bin
	install $(EXE) ../bin/

.PHONY: clean

clean:
	rm -rf $(ODIR) $(EXE)
	mkdir $(ODIR)
