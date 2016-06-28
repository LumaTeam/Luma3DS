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

PinCont readPin(void)
{
	FIL file;
	PinCont temp;

	if(f_open(&file, "/luma/pin.bin", FA_READ) == FR_OK)
    {
        unsigned int read;
        u32 size = f_size(&file);
        f_read(&file, &temp, size, &read);
        f_close(&file);
    }

    return temp;
}

void writePin(PinCont* pin)
{
	FIL file;

    if(f_open(&file, "/luma/pin.bin", FA_WRITE | FA_OPEN_ALWAYS) == FR_OK)
    {
        unsigned int written;
        f_write(&file, pin, sizeof(pin), &written);
        f_close(&file);
    }
}

bool doesPinExist(void)
{
	FIL file;
	bool result = true;

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

void deletePin(void)
{
	// need a way to delete a file.
	// cant get f_unlink to work.

	//f_unlink("/luma/pin.bin");
}

void newPin(void)
{
	clearScreens();

	drawString("Enter your NEW PIN: ", 10, 10, COLOR_WHITE);

	u32 pressed = 0;

    // Set the default characters as 'n' so we can check if there are any unentered characters.
    PinCont enteredPassword;
    for (int i = 0; i < 4; i++)
    {
    	enteredPassword.pin[i] = 'n';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'A';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'B';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'X';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'Y';
                        checking = false;
                    }
                    cnt++;
                }
                
                // visualize character on screen.
                drawCharacter('Y', 10 + charDrawPos, 10, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;
        }

        if (enteredPassword.pin[0] == 'n' || enteredPassword.pin[1] == 'n' || enteredPassword.pin[2] == 'n' || enteredPassword.pin[3] == 'n')
        {
            running = true;
        }
        else
        {
            writePin(&enteredPassword);
            running = false;
        }
    }
}

void verifyPin(void)
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

    // Set the default characters as 'n' so we can check if there are any unentered characters.
    PinCont enteredPassword;
    for (int i = 0; i < 4; i++)
    {
    	enteredPassword.pin[i] = 'n';
    }

    bool running = true;
    bool unlock = true;
    int charDrawPos = 35;

    PinCont setPassword = readPin();

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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'A';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'B';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'X';
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
                    if (enteredPassword.pin[cnt] == 'n')
                    {
                        enteredPassword.pin[cnt] = 'Y';
                        checking = false;
                    }
                    cnt++;
                }
                
                drawCharacter('Y', 10 + charDrawPos, 20, COLOR_WHITE);
                charDrawPos = charDrawPos + 15;
                break;

            case BUTTON_START:
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
                break;
        }

        if (enteredPassword.pin[0] == 'n' || enteredPassword.pin[1] == 'n' || enteredPassword.pin[2] == 'n' || enteredPassword.pin[3] == 'n')
        {
            running = true;
        }
        else
        {
            for (int i = 0; i < 4; i++)
            {
                if (setPassword.pin[i] != enteredPassword.pin[i])
                {
                    unlock = false;
                }
            }

            if (!unlock)
            {   
                // reset
                running = true;
                unlock = true;

                for (int i = 0; i < 4; i++)
                {
                    enteredPassword.pin[i] = 'n';
                }

                pressed = 0;

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