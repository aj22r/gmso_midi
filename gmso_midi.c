#include <stdio.h>
#include <string.h>

#define ROWS 14
#define COLUMNS 400

typedef unsigned __int8 uchar;
typedef unsigned __int16 ushort;
typedef unsigned __int32 uint;

char play(uchar track);

FILE *instream,*outstream;

uchar playing = 0;
uint bpm = 120;
int lastval = 0;
int div = 0x180;
int delaythresh = 0;
int notesplayed = 0;
int columncounter = 0;
char map[COLUMNS][ROWS];

int main(int argc, char *argv[]) {
	if(argc <= 2) {
		printf("Usage: gmso_midi input.mid output.AngryLegGuy\n");
		return 1;
	}
	if((instream = fopen(argv[1], "r")) == NULL) {
		printf("Failed to open %s\n", argv[1]);
		return 1;
	}
	if((outstream = fopen(argv[2], "w")) == NULL) {
		printf("Failed to create %s\n", argv[2]);
		return 1;
	}
	fwrite("GMSO1", sizeof(char), 5, outstream);
	memset(map, '0', sizeof(map));
	while(!notesplayed) {
		__int8 played = play(1);
		printf("Done: %d, notes played: %d\n", played, notesplayed);
		if(played < 0) break;
	}
	char bpmbuf[3];
	sprintf(bpmbuf, "%03u", bpm);
	fwrite(bpmbuf, sizeof(char), sizeof(bpmbuf), outstream);
	fwrite(map, sizeof(char), sizeof(map), outstream);
	_fcloseall();
	return 0;
}

uchar readbyte() {
	uchar byte;
	playing = fread(&byte, sizeof(uchar), 1, instream);
	return byte;
}

ushort readword() {
	ushort word;
	playing = fread(&word, sizeof(ushort), 1, instream);
	return (word>>8) | (word<<8);
}

uint readdword() {
	uint dword;
	playing = fread(&dword, sizeof(uint), 1, instream);
	return ((dword>>24)&0xff) | ((dword<<8)&0xff0000) | ((dword>>8)&0xff00) | ((dword<<24)&0xff000000);
}

uint readvarlen(uchar lastbyte) {
	uint ret = lastbyte & 0x7F;
	while(lastbyte & 0x80) {
		ret <<= 7;
		ret |= lastbyte & 0x7F;
		lastbyte = readbyte();
	}
	return ret;
}

void mididelay(uchar enabled) {
	//printf("%d delaying for %d ms\n", enabled, tempo * lastval);
	delaythresh += lastval;
	if(delaythresh >= div/2) {
		delaythresh = 0;
		columncounter++;
		printf("Next block, columns: %d\n", columncounter);
	}
}

__int8 play(uchar track) {
	uint temp = readdword();
	if(temp == 0x4D546864) {
		if(readdword() != 6) return -2;
		if(readword() > 1) return -3;
		readword();
		div = readword();
		if(div & 0x8000) return -4;
	} else if(temp != 0x4D54726B) {
		return -5;
	}
	uint size = readdword();
	lastval = readvarlen(readbyte());
	uchar nodelay = 1;
	//uchar playing = 1;
	delaythresh = 0;
	notesplayed = 0;
	//tempo = 5000 / div; //ms
	while(playing) {
		if(columncounter >= COLUMNS) {
			break;
		}
		uchar temp = readbyte();
		nodelay = 0;
		if(temp == 0xFF) {
				temp = readbyte();
				switch(temp) {
					case 0:
						mididelay(nodelay);
						readbyte();
						break;
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
					case 0x7F:
						mididelay(nodelay);
						uchar skip = readbyte();
						for(ushort i = 0; i < skip; i++) {
							readbyte();
						}
						lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
					case 0x20:
						mididelay(nodelay);
						readword();
						lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
					case 0x2F:
						mididelay(nodelay);
						if(readbyte() == 0) {
							playing = 0;
						}
						//lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
					case 0x51:
						mididelay(nodelay);
						bpm = 60000 / ((readdword() & 0xFFFFFF) / 1000);
						printf("Set BPM to %d\n", bpm);
						lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
					case 0x54:
						mididelay(nodelay);
						readdword();
						readword();
						lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
					case 0x58:
						mididelay(nodelay);
						readdword();
						readbyte();
						lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
					case 0x59:
						mididelay(nodelay);
						readword();
						readbyte();
						lastval = readvarlen(readbyte());
						nodelay = 0;
						break;
				}
		} else if((temp & 0xF0) == 0x80) { //note off
			mididelay(nodelay);
			uchar note = readbyte();
			readbyte();
			
			//printf("Note off: %d\n", note);
			
			lastval = readvarlen(readbyte());
			nodelay = 0;
		} else if((temp & 0xF0) == 0x90) { //note on
			mididelay(nodelay);
			uchar note = readbyte();
			readbyte();
			
			printf("Note on: %d\n", note);
			notesplayed++;
			note -= 7; //offset

			if(note >= 48 && note <= 71) {
				note -= 48;
				if(note <= 4 || (note >= 12 && note <= 16)) {
					uchar notetype = '1';
					if(note & 1) {
						notetype = '2';
					}
					map[columncounter][ROWS - note / 2 - 1] = notetype;
				} else {
					uchar notetype = '1';
					if(note & 1 == 0) {
						notetype = '2';
					}
					map[columncounter][ROWS - note / 2 - 1] = notetype;
				}
				//printf("Placed note at %d\n", ROWS - note / 2 - 1);
			}
			
			lastval = readvarlen(readbyte());
			nodelay = 0;
		} else if((temp & 0xF0) == 0xA0 || (temp & 0xF0) == 0xB0 || (temp & 0xF0) == 0xE0 || temp == 0xF2) {
			mididelay(nodelay);
			readword();
			lastval = readvarlen(readbyte());
			nodelay = 0;
		} else if((temp & 0xF0) == 0xC0 || (temp & 0xF0) == 0xD0 || temp == 0xF3) {
			mididelay(nodelay);
			readbyte();
			lastval = readvarlen(readbyte());
			nodelay = 0;
		} else if(temp == 0xF0) {
			mididelay(nodelay);
			uchar skip = readbyte();
			for(uchar i = 0; i < skip; i++) {
				readbyte();
			}
			readbyte();
			lastval = readvarlen(readbyte());
			nodelay = 0;
		} else {
			lastval = readvarlen(temp);
			nodelay = 1;
		}
		mididelay(!nodelay);
	}
	return 0;
}