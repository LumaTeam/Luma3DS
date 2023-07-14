# Luma3DS with Timelock

This modified build (available on the `timelock` branch) contains a Timelock system.
It enables Rosalina (Luma3DS's custom sysmodule) to lock the console after a certain time with a PIN code.
Something which is missing from the original parental control, in my opinion.
The main use is for parents not wanting their kids to use the console for too long.

It provides a new config menu in Rosalina letting the user handle the following options:
- Enabling/Disabling the timelock system
- Set the time limit
- Set the PIN code

It uses a config file storing the properties listed above, as well as an elapsed time value to keep track of the time spent using the console. This lets us keep track of the usage time even after a reboot.

**Default PIN: 0000**


Here are some screenshots:

1. Rosalina menu
![1-RosalinaMenu](https://user-images.githubusercontent.com/1681898/159120680-ad2e813b-1337-4744-afab-d9f224620058.jpg)

2. Locked timelock menu
![2-LockedTimelockMenu](https://user-images.githubusercontent.com/1681898/159120683-0a4eccdb-2af9-48a7-95f1-362e3be3ce1c.jpg)

3. Unlocking timelock menu
![3-UnlockingTimelockMenu](https://user-images.githubusercontent.com/1681898/159120687-010fb016-0e05-4bc2-895a-e37400cc0428.jpg)

4. Unlocked timelock menu
![4-UnlockedTimelockMenu](https://user-images.githubusercontent.com/1681898/159120691-b2a78805-6dab-4ad9-9ca5-4b6b4e0f82ba.jpg)

5. Timelocked console
![5-Timelocked](https://user-images.githubusercontent.com/1681898/159120695-cddaba00-8644-4d8c-876a-02518a878cba.jpg)
