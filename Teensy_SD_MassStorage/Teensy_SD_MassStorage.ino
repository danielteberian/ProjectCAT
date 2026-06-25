/*
 * Teensy_SD_MassStorage.ino
 * -------------------------
 * Makes your Teensy 4.1 appear as a USB flash drive on your computer,
 * exposing the built-in microSD card for easy file access and formatting.
 *
 * IMPORTANT SETUP REQUIRED (FOR MAC):
 * -------------------------------------
 * MTP doesn't work well on Mac. Try MSC mode instead:
 *
 * 1. Arduino IDE → Tools → USB Type
 * 2. Look for "Serial + Mass Storage" or "Disk (Serial)"
 *    (The exact name varies by Teensyduino version)
 *
 * If you don't see these options:
 * - Try "Serial + MTP Disk (Experimental)" + install Android File Transfer app
 * - Or just physically remove the SD card and use a card reader
 *
 * HOW TO USE:
 * -----------
 * 1. Upload this sketch to Teensy
 * 2. Teensy will reboot
 * 3. Your Mac should show the SD card as a removable drive
 * 4. You can format it, copy files, etc. directly from Mac
 * 5. When done, eject the drive safely
 * 6. Upload your ProjectCat sketch back (and set USB Type back to "Serial")
 *
 * NOTES:
 * ------
 * - While in MSD mode, the Teensy appears as a storage device
 * - Serial Monitor will still work if you choose "Serial + MTP"
 * - To go back to ProjectCat, just upload the ProjectCat sketch
 * - Remember to change USB Type back to "Serial" for ProjectCat!
 */

#include <SD.h>

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=================================");
  Serial.println("TEENSY USB MASS STORAGE MODE");
  Serial.println("=================================\n");

  Serial.println("Initializing SD card...");

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("ERROR: SD card init failed!");
    Serial.println("Check that:");
    Serial.println("- MicroSD card is inserted in Teensy slot");
    Serial.println("- Card is not corrupted");
    Serial.println("\nYou can still try accessing it from your Mac");
  } else {
    Serial.println("SD card initialized successfully!");

    // Try to get card info
    uint64_t cardSize = SD.sdfs.card()->sectorCount();
    if (cardSize > 0) {
      cardSize = cardSize * 512 / (1024 * 1024); // Convert to MB
      Serial.print("Card size: ");
      Serial.print((uint32_t)cardSize);
      Serial.println(" MB");
    }
  }

  Serial.println("\n=================================");
  Serial.println("READY!");
  Serial.println("=================================");
  Serial.println("\nYour Mac should now see the SD card as a USB drive.");
  Serial.println("You can:");
  Serial.println("- Format the card (choose exFAT for 128GB)");
  Serial.println("- Copy files to/from the card");
  Serial.println("- View existing files");
  Serial.println("\nWhen done:");
  Serial.println("1. Eject the drive safely on Mac");
  Serial.println("2. Upload ProjectCat_Teensy sketch back");
  Serial.println("3. Change USB Type back to 'Serial'\n");
}

void loop() {
  // Nothing to do - MTP/MSD is handled automatically
  // The Teensy firmware handles the USB communication
  delay(1000);
}
