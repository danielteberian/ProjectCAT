# Teensy SD Card - USB Mass Storage Mode

This project turns your Teensy 4.1 into a USB flash drive, letting you access the built-in microSD card directly from your Mac.

## 🚀 Quick Start

### Step 1: Change USB Type Setting

**BEFORE uploading, you MUST change this setting:**

1. Open `Teensy_SD_MassStorage.ino` in Arduino IDE
2. Go to **Tools → USB Type**
3. Select **"Serial + MTP Disk (Experimental)"**
   - OR **"MTP Disk (Experimental)"** (no Serial Monitor)
4. Click **Upload**

### Step 2: Access SD Card on Mac

1. After upload completes, Teensy will reboot
2. **Wait 5-10 seconds**
3. Your Mac should show a new drive (like a USB flash drive)
4. You can now:
   - **Format the card** (Right-click → Format → Choose **exFAT**)
   - Copy files to/from the card
   - Delete files
   - View contents

### Step 3: Return to ProjectCat

When you're done with the SD card:

1. **Eject the drive safely** on Mac (drag to trash or right-click → Eject)
2. Open `ProjectCat_Teensy.ino` in Arduino IDE
3. **Tools → USB Type → Select "Serial"** (or "Serial + MIDI")
4. Upload ProjectCat sketch
5. Your synth is back!

---

## ⚠️ Important Notes

- **MTP mode is experimental** - if it doesn't work, try formatting the card using a USB card reader instead
- **Some Macs may not detect MTP** - if the drive doesn't appear, try:
  - Unplugging and replugging USB
  - Using a different USB port
  - Checking if Android File Transfer is installed (may interfere)
- **Always eject safely** before switching back to ProjectCat
- **Card must be inserted** in Teensy's microSD slot

---

## 🔧 Troubleshooting

**Drive doesn't appear on Mac:**
- Check that USB Type is set to "MTP Disk" or "Serial + MTP"
- Try unplugging and replugging USB cable
- Check Serial Monitor (if using "Serial + MTP") for error messages
- Some Macs need Android File Transfer app for MTP devices

**Can't format large card (128GB):**
- Use **exFAT** format (not FAT32)
- FAT32 only works up to 32GB

**Upload fails:**
- Press the white button on Teensy to enter bootloader mode
- Try again

---

## 📝 What This Does

This sketch configures the Teensy to act as a USB Mass Storage Device (like a flash drive), exposing the SD card to your computer. The MTP (Media Transfer Protocol) mode is handled automatically by the Teensy's USB firmware - no complex code needed!

---

## 🔙 Switching Back

To return to your ProjectCat synth:

1. Eject the SD card drive from Mac
2. Open ProjectCat_Teensy.ino
3. **Change USB Type back to "Serial"**
4. Upload

That's it!
