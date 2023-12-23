#include "itcm.h"
#include "fs.h"
#include "utils.h"
#include "memory.h"

/* Patches the ITCM with the OTP provided,
 * functionally bypassing error 022-2812.
 */
void patchITCM() {
    Otp otp;
    u32 otpSize = fileRead(&otp, OTP_PATH, sizeof(Otp));

    // Error checking
    if (otpSize != sizeof(Otp)) error("OTP is not the correct size.");
    if (otp.magic != OTP_MAGIC) error("Unable to parse OTP. Is it decrypted properly?");

    // Setting relevant values in memory to struct parsed from file
    ARM9_ITCM->otp.deviceId = otp.deviceId;
    ARM9_ITCM->otp.timestampYear = otp.timestampYear;
    ARM9_ITCM->otp.timestampMonth = otp.timestampMonth;
    ARM9_ITCM->otp.timestampDay = otp.timestampDay;
    ARM9_ITCM->otp.timestampHour = otp.timestampHour;
    ARM9_ITCM->otp.timestampMinute = otp.timestampMinute;
    ARM9_ITCM->otp.timestampSecond = otp.timestampSecond;
    ARM9_ITCM->otp.ctcertExponent = otp.ctcertExponent;
    memcpy(ARM9_ITCM->otp.ctcertPrivK, otp.ctcertPrivK, sizeof(otp.ctcertPrivK));
    memcpy(ARM9_ITCM->otp.ctcertSignature, otp.ctcertSignature, sizeof(otp.ctcertSignature));
}
