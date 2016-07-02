/*
*   pin.c
*   By reworks.
*/

#include "draw.h"
#include "screen.h"
#include "utils.h"
#include "buttons.h"
#include "fatfs/ff.h"
#include "i2c.h"
#include "pin.h"

// Use ECB
#define ECB 1
#include "aes/aes.h"

void readPin(uint8_t* out)
{
	// example key. would need to be stored more securely.
	uint8_t key[] = {0xc9, 0x43, 0x7a, 0x92, 0x76, 0x5e, 0x64, 0x9f, 0x6f, 0x4c, 0x33, 0xb4, 0x5c, 0x30, 0x15, 0x97};

	FIL file;

	// AES is 128bit, even though our pin is 32bit. So we have to pad it with zeros.
	uint8_t in[16];
	//uint8_t temp[16];

	if(f_open(&file, "/luma/pin.bin", FA_READ) == FR_OK)
    {
        unsigned int read;
        u32 size = f_size(&file);
        f_read(&file, &in, size, &read);
        f_close(&file);
    }

    AES128_ECB_decrypt(in, key, out);
}

void writePin(uint8_t* in)
{
	// example key. would need to be stored more securely.
	uint8_t key[] = {0xc9, 0x43, 0x7a, 0x92, 0x76, 0x5e, 0x64, 0x9f, 0x6f, 0x4c, 0x33, 0xb4, 0x5c, 0x30, 0x15, 0x97};

	FIL file;

	uint8_t out[16];

	AES128_ECB_encrypt(in, key, out);

    if(f_open(&file, "/luma/pin.bin", FA_WRITE | FA_OPEN_ALWAYS) == FR_OK)
    {
        unsigned int written;
        f_write(&file, &out, sizeof(out), &written);
        f_close(&file);
    }
}

bool doesPinExist(void)
{
	bool result = true;
	FIL file;
	
	if(f_open(&file, "/luma/pin.bin", FA_READ) == FR_OK)
	{
		f_close(&file);
	}
	else
	{
		result = false;
	}	

	return result;
}

/*
bool validateFile(void)
{	
	bool result = true;

	uint8_t in[16];
	readPin(in);

	if (in[16] != 0x65)
	{
		result = false;
	}
}
*/

void deletePin(void)
{
	// To get around f_unlink.

	uint8_t in[16];
    readPin(in);

    for (int i = 0; i < 16; i++)
    {
    	in[i] = 0x00;
    }

    writePin(in);
}

void newPin(void)
{
	clearScreens();

	drawString("Enter your NEW PIN: ", 10, 10, COLOR_WHITE);

	u32 pressed = 0;

    // Set the default value as 0x00 so we can check if there are any unentered characters.
    uint8_t enteredPassword[16];

    for (int i = 0; i < 16; i++)
    {
    	enteredPassword[i] = 0x00;
    }

    bool running = true;

    int charDrawPos = 155;

    while (running == true)
    {
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & PIN_BUTTONS));

        // I hate C89.
        int cnt = 0;
        bool checking = true;

        switch (pressed)
        {
            case BUTTON_A:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x41;
                        checking = false;
                    }
                    cnt++;
                }

                // visualize character on screen.
                drawCharacter('A', 10 + charDrawPos, 10, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_B:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x42;
                        checking = false;
                    }
                    cnt++;
                }
                
                // visualize character on screen.
                drawCharacter('B', 10 + charDrawPos, 10, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_X:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x58;
                        checking = false;
                    }
                    cnt++;
                }
                
                // visualize character on screen.
                drawCharacter('X', 10 + charDrawPos, 10, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_Y:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x59;
                        checking = false;
                    }
                    cnt++;
                }
                
                // visualize character on screen.
                drawCharacter('Y', 10 + charDrawPos, 10, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;
        }

        // we leave the rest of the array zerod out, we only need the first 4 elements.
        if (enteredPassword[0] == 0x00 || enteredPassword[1] == 0x00 || enteredPassword[2] == 0x00 || enteredPassword[3] == 0x00)
        {
            running = true;
        }
        else
        {
        	// Add check
        	enteredPassword[16] = 0x65;

            writePin(enteredPassword);
            running = false;
        }
    }
}

void verifyPin(bool allowQuit)
{
    u32 needToDeinit = 0;

    //if screens are not inited
    if(PDN_GPU_CNT == 1)
    {
    	needToDeinit = initScreens();
    }

    clearScreens();

    drawString("Press START to shutdown or enter pin to proceed.", 10, 10, COLOR_WHITE);
    drawString("Pin: ", 10, 20, COLOR_WHITE);

    u32 pressed = 0;

    // Set the default characters as 0x00 so we can check if there are any unentered characters.
    uint8_t enteredPassword[16];
    for (int i = 0; i < 16; i++)
    {
    	enteredPassword[i] = 0x00;
    }

    bool running = true;
    bool unlock = true;
    int charDrawPos = 35;

    uint8_t setPassword[16];
    readPin(setPassword);

    while (running == true)
    {
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & PIN_BUTTONS));

        // I hate C89.
        int cnt = 0;
        bool checking = true;

        switch (pressed)
        {
            case BUTTON_A:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x41;
                        checking = false;
                    }
                    cnt++;
                }

                // visualize character on screen.
                drawCharacter('A', 10 + charDrawPos, 20, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_B:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x42;
                        checking = false;
                    }
                    cnt++;
                }
                
                // visualize character on screen.
                drawCharacter('B', 10 + charDrawPos, 20, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_X:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x58;
                        checking = false;
                    }
                    cnt++;
                }
                
                drawCharacter('X', 10 + charDrawPos, 20, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_Y:
                // loop through, checking for empty slot.
                // Avoid using 'break' in a for loop.
                while (checking)
                {
                    // add character to password.
                    if (enteredPassword[cnt] == 0x00)
                    {
                        enteredPassword[cnt] = 0x59;
                        checking = false;
                    }
                    cnt++;
                }
                
                drawCharacter('Y', 10 + charDrawPos, 20, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_START:
                if (allowQuit)
                {
                	// exit, no password
	                if(needToDeinit)
	                {
	                     //Turn off backlight
	                    i2cWriteRegister(I2C_DEV_MCU, 0x22, 0x16);
	                    deinitScreens();
	                    PDN_GPU_CNT = 1;
	                }

	                // I think this is the correct shutdown procedure. Not sure.
	                i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
	            }
                break;
        }

        if (enteredPassword[0] == 0x00 || enteredPassword[1] == 0x00 || enteredPassword[2] == 0x00 || enteredPassword[3] == 0x00)
        {
            running = true;
        }
        else
        {
        	// only compare first 4
            for (int i = 0; i < 4; i++)
            {
                if (setPassword[i] != enteredPassword[i])
                {
                    unlock = false;
                }
            }

            if (!unlock)
            {   
                // reset
                running = true;
                unlock = true;

                // re zero out all 16 just in case.
                for (int i = 0; i < 16; i++)
                {
                    enteredPassword[i] = 0x00;
                }

                pressed = 0;
                charDrawPos = 35;
                
                clearScreens();
          
    			drawString("Press START to shutdown or enter pin to proceed.", 10, 10, COLOR_WHITE);
    			drawString("Pin: ", 10, 20, COLOR_WHITE);
    			drawString("Wrong pin! Try again!", 10, 30, COLOR_RED); 
            }
            else
            {
                running = false;
            }
        }
    }  
}