#include <stdio.h>
int main(int argc, const char *argv[])
{
	FILE *file;
	unsigned int state;
	unsigned short data, prevdata = 1;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s input-file\n", argv[0]);
		return 255;
	}

	if ((file = fopen(argv[1], "rb")) == NULL)
	{
		perror("fopen");
		return 1;
	}

	while (fread(&data, 2, 1, file) == 1)
	{
		if (data == 0){
			printf("\\0");
		}else if (data == '\r' || data == '\n'){
			printf("\\%03o", data);
		}else if (((prevdata == 0) && (data >= '0') && (data <= '7'))){
			printf("00%c", data);
		}else if ((data == '\'') || /*(data == '"') ||*/ (data == '\\')){
			printf("\\%c", (char)data);
		}else if (data == 0x2028 || ((data >= 0xD800) && (data <= 0xDFFF))){
			printf("\\u%04x", data);
		}else if (data < 0x80){
			printf("%c", (char)data);
		}else if (data < 0x800){
			printf("%c%c", ((data >> 6) & 0x1F) | 0xC0, (data & 0x3F) | 0x80);
		}else{
			printf("%c%c%c",  ((data >> 12) & 0x0F) | 0xE0, ((data >> 6) & 0x3F) | 0x80, ((data) & 0x3F) | 0x80);
		}
		prevdata = data;
	}
	fclose(file);
	return 0;
}
