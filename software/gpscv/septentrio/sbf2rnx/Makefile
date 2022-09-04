PROGRAM = sbf2rnx
CC = gcc


FLAGS	= -O -mno-align-double -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -DNO_DECRYPTION

LDFLAGS = -lm 

SBF_OBJS	= sbfread.o sbfread_meas.o sbfsvid.o ssngetop.o crc.o


sbf2rnx: sbf2rnx.o $(SBF_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o	: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean	:
	rm -f $(PROGRAM) $(ALL_OBJS) 

# Source dependencies:

sbf2asc.o         : sbf2asc.c ssngetop.h sbfread.h measepoch.h measepochconfig.h sviddef.h sbfsvid.h ssntypes.h sbfdef.h sbfsigtypes.h sbf2asc_version.h

sbfread.o         : sbfread.c sbfread.h crc.h ssntypes.h sbfdef.h sbfsigtypes.h

sbfread_meas.o    : sbfread_meas.c sbfread.h measepoch.h measepochconfig.h sviddef.h sbfsvid.h crc.h ssntypes.h sbfdef.h sbfsigtypes.h

ssngetop.o        : ssngetop.c ssngetop.h

crc.o             : crc.c crc.h ssntypes.h sbfdef.h

sbf2rnx.o : sbf2rnx.c sbfread.h measepoch.h measepochconfig.h sviddef.h sbfsvid.h ssntypes.h sbfdef.h sbfsigtypes.h

# End of Makefile



