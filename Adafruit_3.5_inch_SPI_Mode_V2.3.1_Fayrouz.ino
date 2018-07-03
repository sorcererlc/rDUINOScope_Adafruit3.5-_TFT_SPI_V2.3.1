//  
//    rDUINOScope - Arduino based telescope control system (GOTO).
//    Copyright (C) 2016 Dessislav Gouzgounov (Desso)
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    any later version.
//
//    PROJECT Website: http://rduinoscope.byethost24.com
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//    ALLIGNMENT Method: 1 Star Alignment - The method I have implemented is part of Mr. Ralph Pass alignment procedure described on http://rppass.com/  
//                       Mr. Pass helped rDUINOScope by providing the calculations needed to implement the method. http://rppass.com/align.pdf - the actual PDF
//                       
//                       Iterative Alignment - The method is based on article from "New" Hamilton Astronomy.com website: http://astro.hcadvantage.com
//                       Actual PDF document: http://astro.hcadvantage.com/ASTRO_ARTICLES/Polar_Alignment_Part_II.pdf
//          
//
//

// HERE GOES THE Mount, Gears and Drive information.
// ... used to calculate the HourAngle to microSteps ratio
// UPDATE THIS PART according to your SET-UP
// ---------------------------------------------
// NB: RA and DEC uses the same gear ratio (144 tooth in my case)!
//----------------------------------------------
#ifdef serial_debug
  int WORM = 144;
#else
  int WORM = 144;
#endif
int REDUCTOR = 4;      // 1:4 gear reduction
int DRIVE_STP = 200;   // Stepper drive have 200 steps per revolution
int MICROSteps = 16;   // I'll use 1/16 microsteps mode to drive sidereal - also determines the LOWEST speed.

// below variables are used to calculate the paramters where the drive works
int ARCSEC_F_ROTAT = 1296000;    // ArcSeconds in a Full earth rotation;
float SIDEREAL_DAY = 86164.0905;   // Sidereal day in seconds
float ArcSECstep;
int MicroSteps_360;
int RA_90;  // How much in microSteps the RA motor have to turn in order to make 6h = 90 degrees;
int DEC_90;   // How mich in microSteps the DEC motor have to turn in order to make 6h = 90 degrees;
int HA_H_CONST;
int HA_M_CONST;
int DEC_D_CONST;
int DEC_M_CONST;
int MIN_TO_MERIDIAN_FLIP = 2;   // This constant tells the system when to do the Meridian Flip. "= 2" means 2 minutes before 24:00h (e.g. 23:58h)
int MIN_SOUND_BEFORE_FLIP = 3;   // This constant tells the system to start Sound before it makes Meridian Flip
float mer_flp;                   // The calculateLST_HA() function depending on this timer will convert the HA and DEC to the propper ones to do the flip.
boolean MERIDIAN_FLIP_DO = false;
int Tracking_type = 1;  // 1: Sidereal, 2: Solar, 0: Lunar;
int Clock_Sidereal;  // Variable for the Interruptions. nterruption is initialized depending on the DATA above -in miliseconds
int Clock_Solar;  // Variable for the Interruptions. nterruption is initialized depending on the DATA above -in miliseconds
int Clock_Lunar;  // Variable for the Interruptions. nterruption is initialized depending on the DATA above -in miliseconds

////////////////////////////////////////////////
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_HX8357.h"
#include "DHT.h"
#include <TinyGPS++.h>
#include <Time.h>
#include "TouchScreen.h"
#include <SD.h>
#include <DueTimer.h> // interruptions library0
#include <DS3231.h>
#include <math.h>
#include <TimeLib.h>

// These are the four touchscreen analog pins
#define YP A10  // Y+ to A10   >>> must be an analog pin
#define XM A11  // X- to A11   >>> must be an analog pin
#define YM 44   // Y- to 44    >>> a digital pin
#define XP 42   // X+ to 42    >>> a digital pin

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 120  // touch Left margin  >>> Default value is 110 
#define TS_MINY 90   // touch up margin    >>> Default value is 80
#define TS_MAXX 850  // touch Right margin >>> Default value is 900  
#define TS_MAXY 930  // touch Down margin  >>> Default value is 940 
 
#define MINPRESSURE 5    // Minimum Pressure Touch Sensitivity >>> Default value is 10
#define MAXPRESSURE 1000 // Maximum Pressure Touch Sensitivity >>> Default value is 1000

// The display uses hardware SPI, plus #43 & #45
#define TFT_RST -1  // tie to arduino SPI RST 
#define TFT_DC 3    // khairey changed it from 43  to   3
#define TFT_CS 52

int sd_cs = 21;  // Khairey moved it from 4 to 41  // sd card chip select pin is Arduino digital pin 4
#define DHTPIN 43   // khairey changed it from  3  to   43
#define DHTTYPE DHT22


#define reverse_logic false //set true if the stepper drivers logic is "neglected enabled"
//#define serial_debug    // comment out to deactivate the serial debug mode

DS3231 rtc(A4, A5);           // (SDA-A4, SCL-A5) from the RTC board
DHT dht(DHTPIN, DHTTYPE);
TinyGPSPlus gps;             // (Rx-16, Tx-17) 

/** NIGHT MODE ANALOG pin */
#define DAY_NIGHT_PIN A6

//Joystick Pins//
int yPin = A0;
int xPin = A1;
int Joy_SW = A9;
int x_cal, y_cal = 0;

int FAN1 = A2;
int FAN2 = A3;

int speakerOut = DAC1;  // Was 2
int TFTBright = DAC0;

int POWER_DRV8825 = A8;

int RA_DIR = 5;   
int RA_STP = 4;     
int RA_MODE2 = 13;  
int RA_MODE1 = 12;   
int RA_MODE0 = 11;   

int DEC_DIR = 7;     
int DEC_STP = 6;     
int DEC_MODE2 = 8;   
int DEC_MODE1 = 9;   
int DEC_MODE0 = 10;   

Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Assign human-readable names to some common 16-bit color values:
#define BLACK           0x0000      /*   0,   0,   0 */
#define NAVY            0x000F      /*   0,   0, 128 */
#define DarkGreen       0x03E0      /*   0, 128,   0 */
#define DarkCyan        0x03EF      /*   0, 128, 128 */
#define Maroon          0x7800      /* 128,   0,   0 */
#define Purple          0x780F      /* 128,   0, 128 */
#define Olive           0x7BE0      /* 128, 128,   0 */
#define LightGrey       0xC618      /* 192, 192, 192 */
#define DarkGrey        0x7BEF      /* 128, 128, 128 */
#define Blue            0x001F      /*   0,   0, 255 */
#define GREEN           0x07E0      /*   0, 255,   0 */
#define Cyan            0x07FF      /*   0, 255, 255 */
#define RED             0xF800      /* 255,   0,   0 */
#define Magenta         0xF81F      /* 255,   0, 255 */
#define YELLOW          0xFFE0      /* 255, 255,   0 */
#define WHITE           0xFFFF      /* 255, 255, 255 */
#define Orange          0xFD20      /* 255, 165,   0 */
#define GreenYellow     0xAFE5      /* 173, 255,  47 */
#define Purple            0xF81F

#define  note_c     3830    // 261 Hz 
#define  note_cb    3610    // 261 Hz 
#define  note_d     3400    // 294 Hz 
#define  note_e     3038    // 329 Hz 
#define  note_f     2864    // 349 Hz 
#define  note_g     2550    // 392 Hz 
#define  note_gb    2411    // 392 Hz 
#define  note_a     2272    // 440 Hz 
#define  note_b     2028    // 493 Hz 
#define  note_C     1912    // 523 Hz 
#define  note_X     1650    // 
//
//
const String FirmwareDate = "26 06 18";
const String FirmwareNumber = "v2.3.1 Boiana EQ-Fayrouz";
const String FirmwareName = "rDUINOScope";
const String FirmwareTime = "12:00:00";
//
String Messier_Array[120];
String Treasure_Array[130];
String ObservedObjects[50];
String Iter_Stars[50];
int int_star_count = 0;       // Counter for how many stars are loaded into Iter_Stars[] array.... to be used with Pages, so that it does not show more pages than needed
String Stars[] = {            // Definition of the ALIGNMENT Stars. Add as many stars as you like in here
"And;Almaak;2h4m;42.3°",
"And;Alpheratz;0h8m;29.1°",
"And;Mirach;1h10m;35.6°",
"Aql;Altair;19h51m;8.9°",
"Aql;Althalimain;19h6m;-4.9°",
"Aql;Deneb el Okab;19h5m;13.9°",
"Aql;Tarazed;19h46m;10.6°",
"Aqr;Sadalmelik;22h6m;-0.3°",
"Aqr;Sadalsuud;21h32m;-5.6°",
"Aqr;Skat;22h55m;-15.8°",
"Ara;Choo;17h32m;-49.9°",
"Ari;Hamal;2h7m;23.5°",
"Ari;Sheratan;1h55m;20.8°",
"Aur;Almaaz;5h2m;43.8°",
"Aur;Capella;5h17m;46°",
"Aur;Hassaleh;4h57m;33.2°",
"Aur;Hoedus II;5h7m;41.2°",
"Aur;Menkalinan;6h0m;44.9°",
"Boo;Arcturus;14h16m;19.2°",
"Boo;Izar;14h45m;27.1°",
"Boo;Muphrid;13h55m;18.4°",
"Boo;Nekkar;15h2m;40.4°",
"Boo;Seginus;14h32m;38.3°",
"Cap;Dabih;20h21m;-14.8°",
"Cap;Deneb Algedi;21h47m;-16.1°",
"Car;Aspidiske;9h17m;-59.3°",
"Car;Avior;8h23m;-59.5°",
"Car;Canopus;6h24m;-52.7°",
"Car;Miaplacidus;9h13m;-69.7°",
"Cas;Achird;0h49m;57.8°",
"Cas;Caph;0h9m;59.2°",
"Cas;Cih;0h57m;60.7°",
"Cas;Ruchbah;1h26m;60.2°",
"Cas;Schedar;0h41m;56.5°",
"Cas;Segin;1h54m;63.7°",
"Cen;Al Nair al Kent;13h56m;-47.3°",
"Cen;Hadar;14h4m;-60.4°",
"Cen;Ke Kwan;14h59m;-42.1°",
"Cen;Menkent;14h7m;-36.4°",
"Cen;Muhlifain;12h42m;-49°",
"Cen;Rigil Kentaurus;14h40m;-60.8°",
"Cep;Alderamin;21h19m;62.6°",
"Cep;Alphirk;21h29m;70.6°",
"Cep;Errai;23h39m;77.6°",
"Cet;Dheneb;1h9m;-10.2°",
"Cet;Diphda;0h44m;-18°",
"Cet;Kaffaljidhma;2h43m;3.2°",
"Cet;Menkar;3h2m;4.1°",
"CMa;Adhara;6h59m;-29°",
"CMa;Aludra;7h24m;-29.3°",
"CMa;Mirzam;6h23m;-18°",
"CMa;Phurad;6h20m;-30.1°",
"CMa;Sirius;6h45m;-16.7°",
"CMa;Wezen;7h8m;-26.4°",
"CMi;Gomeisa;7h27m;8.3°",
"CMi;Procyon;7h39m;5.2°",
"Cnc;Tarf;8h17m;9.2°",
"Col;Phact;5h40m;-34.1°",
"Col;Wazn;5h51m;-35.8°",
"CrB;Alphecca;15h35m;26.7°",
"Cru;Acrux;12h27m;-63.1°",
"Cru;Gacrux;12h31m;-57.1°",
"Cru;Mimosa;2h48m;-59.7°",
"Crv;Algorel;12h30m;-16.5°",
"Crv;Gienah Ghurab;12h16m;-17.5°",
"Crv;Kraz;12h34m;-23.4°",
"Crv;Minkar;12h10m;-22.6°",
"CVn;Cor Caroli;12h56m;38.3°",
"Cyg;Albireo;19h31m;28°",
"Cyg;Deneb;20h41m;45.3°",
"Cyg;Gienah;20h46m;34°",
"Cyg;Sadr;20h22m;40.3°",
"Dra;Aldhibah;17h9m;65.7°",
"Dra;Aldhibain;16h24m;61.5°",
"Dra;Edasich;15h25m;59°",
"Dra;Eltanin;17h57m;51.5°",
"Dra;Rastaban;17h30m;52.3°",
"Dra;Tais;19h13m;67.7°",
"Eri;Acamar;2h58m;-40.3°",
"Eri;Achernar;1h38m;-57.2°",
"Eri;Kursa;5h8m;-5.1°",
"Eri;Rana;3h43m;-9.8°",
"Eri;Zaurak;3h58m;-13.5°",
"Gem;Alhena;6h38m;16.4°",
"Gem;Alzirr;6h45m;12.9°",
"Gem;Castor;7h35m;31.9°",
"Gem;Mebsuta;6h44m;25.1°",
"Gem;Pollux;7h45m;28°",
"Gem;Propus;6h15m;22.5°",
"Gem;Tejat;6h23m;22.5°",
"Gem;Wasat;7h20m;22°",
"Gru;Al Dhanab;22h43m;-46.9°",
"Gru;Alnair;22h8m;-47°",
"Her;Kornephoros;16h30m;21.5°",
"Her;Rasalgethi;17h15m;14.4°",
"Her;Rutilicus;16h41m;31.6°",
"Her;Sarin;17h15m;24.8°",
"Hya;Alphard;9h28m;-8.7°",
"Hyi;Head of Hydrus;1h59m;-61.6°",
"Ind;Persian;20h38m;-47.3°",
"Leo;Algieba;10h20m;19.8°",
"Leo;Chort;11h14m;15.4°",
"Leo;Denebola;11h49m;14.6°",
"Leo;dhafera;10h17m;23.4°",
"Leo;Ras Elased Austr;9h46m;23.8°",
"Leo;Regulus;10h8m;12°",
"Leo;Subra;9h41m;9.9°",
"Leo;Zosma;11h14m;20.5°",
"Lep;Arneb;5h33m;-17.8°",
"Lep;Nihal;5h28m;-20.8°",
"Lib;Brachium;15h04m;-25.3°",
"Lib;Zubenelgenubi;14h51m;-16°",
"Lib;Zubeneschamali;15h17m;-9.4°",
"Lup;Ke Kouan;14h59m;-43.1°",
"Lup;Men;14h42m;-47.4°",
"Lyr;Sheliak;18h50m;33.4°",
"Lyr;Sulaphat;18h59m;32.7°",
"Lyr;Vega;18h37m;38.8°",
"Oph;Cebalrai;17h43m;4.6°",
"Oph;Han;16h37m;-10.6°",
"Oph;Rasalhague;17h35m;12.6°",
"Oph;Sabik;17h10m;-15.7°",
"Oph;Yed Posterior;16h18m;-4.7°",
"Oph;Yed Prior;16h14m;-3.7°",
"Ori;Algjebbah;5h24m;-2.4°",
"Ori;Alnilam;5h36m;-1.2°",
"Ori;Alnitak;5h41m;-1.9°",
"Ori;Bellatrix;5h25m;6.3°",
"Ori;Betelgeuse;5h55m;7.41°",
"Ori;Hatysa;5h35m;-5.9°",
"Ori;Meissa;5h35m;9.9°",
"Ori;Mintaka;5h32m;-0.3°",
"Ori;Rigel;5h15m;-8.2°",
"Ori;Saiph;5h48m;-9.7°",
"Ori;Tabit;4h50m;7°",
"Pav;Peacock;20h26m;-56.7°",
"Peg;Algenib;0h13m;15.2°",
"Peg;Baham;22h10m;6.2°",
"Peg;Enif;21h44m;9.9°",
"Peg;Homam;22h41m;10.8°",
"Peg;Markab;23h5m;15.2°",
"Peg;Matar;22h43m;30.2°",
"Peg;Sadalbari;22h50m;24.6°",
"Peg;Scheat;23h4m;28.1°",
"Per;Algol;3h8m;41°",
"Per;Gorgonea Tertia;3h5m;38.8°",
"Per;Mirfak;3h24m;49.9°",
"Phe;Ankaa;0h26m;-42.3°",
"PsA;Fomalhaut;22h58m;-29.6°",
"Pup;Asmidiske;7h49m;-24.9°",
"Pup;Naos;8h4m;-40°",
"Pup;Turais;8h8m;-24.3°",
"Sco;Alniyat;16h21m;-25.6°",
"Sco;Antares;16h29m;-26.4°",
"Sco;Dschubba;16h0m;-22.6°",
"Sco;Girtab;17h42m;-39°",
"Sco;Graffias;16h5m;-19.8°",
"Sco;Lesath;17h31m;-37.3°",
"Sco;Sargas;17h37m;-43°",
"Sco;Shaula;17h34m;-37.1°",
"Sco;We;16h50m;-34.3°",
"Ser;Alava;18h21m;-2.9°",
"Ser;Unukalhai;15h44m;6.4°",
"Sgr;Albaldah;9h10m;-21°",
"Sgr;Alnasl;18h6m;-30.4°",
"Sgr;Ascella;19h3m;-29.9°",
"Sgr;Kaus Australis;18h24m;-34.4°",
"Sgr;Kaus Borealis;18h28m;-25.4°",
"Sgr;Kaus Meridionalis;18h21m;-29.8°",
"Sgr;Nunki;18h55m;-26.3°",
"Tau;Ain;4h29m;19.2°",
"Tau;Alcyone;3h47m;24.1°",
"Tau;Aldebaran;4h36m;16.5°",
"Tau;Alheka;5h38m;21.1°",
"Tau;Elnath;5h26m;28.6°",
"TrA;Atria;16h49m;-69°",
"Tri;Mothallah;1h53m;29.6°",
"UMa;Al Haud;9h33m;51.7°",
"UMa;Alioth;12h54m;56°",
"UMa;Alkaid;13h48m;49.3°",
"UMa;Alula Borealis;11h18m;33.1°",
"UMa;Dubhe;11h4m;61.8°",
"UMa;Megrez;12h15m;57°",
"UMa;Merak;11h2m;56.4°",
"UMa;Mizar;13h24m;54.9°",
"UMa;Muscida;8h30m;60.7°",
"UMa;Phecda;11h54m;53.7°",
"UMa;Talita;8h59m;48°",
"UMa;Tania Australis;10h22m;41.5°",
"UMa;Tania Borealis;10h17m;42.9°",
"UMi;Kochab;14h51m;74.2°",
"UMi;Pherkad Major;15h21m;71.8°",
"UMi;Polaris;2h32m;89.3°",
"Vel;Koo She;8h45m;-54.7°",
"Vel;Markeb;9h22m;-55°",
"Vel;Regor;8h10m;-47.3°",
"Vel;Suhail;9h8m;-43.4°",
"Vel;Tseen Ke;9h57m;-54.6°",
"Vir;Auva;12h56m;3.4°",
"Vir;Heze;13h35m;-0.6°",
"Vir;Porrima;12h42m;-1.5°",
"Vir;Spica;13h25m;-11.2°",
"Vir;Vindemiatrix;13h2m;11°"
};
int Observed_Obj_Count = 0;
int ALLIGN_STEP = 0;  // Using this variable to count the allignment steps - 1: Synchronize, 2: Allign and Center, 3:....
int ALLIGN_TYPE = 0;  // Variable to store the alignment type (0-Skip Alignment, 1-1 Star alignment, 2-2 Star alignment
float delta_a_RA = 0;
float delta_a_DEC = 0;
int Iterative_Star_Index = 0;
String Prev_Obj_Start;
int lastScreenUpdateTimer;
unsigned long Slew_timer, Slew_RA_timer = 0;
int OBJECT_Index;
String OBJECT_NAME;
String OBJECT_DESCR;
String OBJECT_DETAILS;
String BT_COMMAND_STR;
String START_TIME;
int STP_FWD = LOW;
int STP_BACK = HIGH;
float OBJECT_RA_H;
float OBJECT_RA_M;
float OBJECT_DEC_D;
float OBJECT_DEC_M;
float OBJECT_MAG;
float curr_RA_H, curr_RA_M, curr_RA_S, curr_DEC_D, curr_DEC_M, curr_DEC_S;    // Global variables to store Mount's current RA and DEC.
char curr_RA_lz[9], curr_DEC_lz[10], curr_HA_lz[9];                                                // Global variable to store current RA and DEC with Leading Zeroes and sign (RA: 00:00:00; DEC: +/-00*00:00)
int SELECTED_STAR = 0;
double DELTA_RA_ADJUST = 1; // cos RA
double DELTA_DEC_ADJUST = 1; // cos DEC
// Default values to load when CANCEL button is hit on the GPS screen
float OBSERVATION_LONGITUDE = 31.0004; // (23.3333* - Home)
float OBSERVATION_LATTITUDE = 30.7865; // (42.6378* - Home)
float OBSERVATION_ALTITUDE = 11.00; // Tanta, Egypt
int TIME_ZONE = 2;
// .............................................................
int GPS_iterrations = 0;
double LST, HAHour, HAMin, ALT, AZ;
double JD;
String BTs;

int last_button, MESS_PAGER, TREAS_PAGER, STARS_PAGER;
boolean IS_TFT_ON = true;
boolean IS_STEPPERS_ON = true;
boolean IS_OBJ_VISIBLE = false;
boolean IS_IN_OPERATION = false;  // This variable becomes True when Main screen appears
boolean IS_TRACKING = false;
boolean IS_NIGHTMODE;
boolean IS_OBJ_FOUND = true;
boolean IS_OBJECT_RA_FOUND = true;
boolean IS_OBJECT_DEC_FOUND = true;
boolean IS_MERIDIAN_PASSED = false;
boolean IS_POSIBLE_MERIDIAN_FLIP = true;
boolean IS_MERIDIAN_FLIP_AUTOMATIC = true;
boolean IS_BT_MODE_ON = false;
boolean IS_MANUAL_MOVE = false;
boolean IS_FAN1_ON = true;
boolean IS_FAN2_ON = true;
boolean IS_CUSTOM_MAP_SELECTED = false;
boolean IS_SOUND_ON = true;
int TFT_Brightness = 255;
int MAIN_SCREEN_MENU = 0;
int CURRENT_SCREEN = 0; 
int LOAD_SELECTOR;   // selector to show which LOADING mechanism is used: 1 - Messier, 2 - File, 3 - NGCs

String Fan1_State="ON";
String Fan2_State="ON";
String TFT_Time="AL-ON";
String Sound_State = "ON";
String Stepper_State = "ON";
String Mer_Flip_State = "Auto";
String Tracking_Mode = "Celest";

int RA_microSteps, DEC_microSteps, rev_RA_microSteps, rev_DEC_microSteps;              // Current position of the motors in MicroSteps! - when movement occures, values are changed accordingly (manual, tracking or slew to);
int RA_mode_steps, DEC_mode_steps;
int SLEW_RA_microsteps, SLEW_DEC_microsteps;    // Where the mottors needs to go in order to point to the object
int RA_finish_last = 0;
  int map_r = 0;    // Used to determine the StarMap Row ... image name (1-1.bmp; 1-2.bmp ....)
  int map_c = 0;    // Ued to determine the StarMap Column .... image name


String old_t, old_d;
String Start_date;
int update_time, Tupdate_time, TFT_timeout;
unsigned long UPD_T, UPD_coord, DELAY_Slew, UPD_LST, TFT_Timer;
int RA_move_ending;
int w_DateTime[12]={0,0,0,0,0,0,0,0,0,0,0,0};  // array to store date - as updated from updater screen - Wishing_Date
int dateEntryPos = 0;
int Summer_Time = 0;
int xPosition = 0;  // Joystick
int yPosition = 0;  // Joystick
float _temp,_Stemp;
float _humid,_Shumid;
int16_t texts, Button_State_ON,Button_State_OFF,Button_Title,l_text, d_text, btn_l_border, btn_d_border, btn_l_text, btn_d_text, btn_l_selection, title_bg, title_texts, messie_btn, file_btn, ngc_btn, MsgBox_bg, MsgBox_t;    // defines string constants for the clor - Depending on the DAY/NIGHT modes
File roots;
File StarMaps;                    // bmp files

// Some variables used for Alignment procedure:
double Star_1_HA = 0;
double Star_1_DEC = 0;
double Star_2_HA = 0;
double Star_2_DEC = 0;
double err_RA = 0;
double err_DEC = 0;
double Eerr_RA = 0;
double Eerr_DEC = 0;
double err_ALT = 0;
double err_AZ = 0;
double det = 0;


void setup(void) {
  #ifdef serial_debug
    Serial.begin(57600);
    while(!Serial) {}
  #endif
  Serial2.begin(9600);  // Initialize GPS communication on PINs: 17 (RX) and 16 (TX) 
  Serial3.begin(9600); // Bluetooth communication on PINs:  15 (RX) and 14 (TX)  
  pinMode(speakerOut, OUTPUT);

  // below variables are used to calculate the paramters where the drive works
  int ww = WORM*REDUCTOR;
  int www = DRIVE_STP*MICROSteps;
 
  MicroSteps_360 = ww*www;
  RA_90 = MicroSteps_360 / 4;  // How much in microSteps the RA motor have to turn in order to make 6h = 90 degrees;
  DEC_90 = RA_90;   // How mich in microSteps the DEC motor have to turn in order to make 6h = 90 degrees;
  HA_H_CONST = MicroSteps_360/360;
  DEC_D_CONST = HA_H_CONST;
  
  Clock_Sidereal = 1000000/(MicroSteps_360/SIDEREAL_DAY);  // This way I make the interruption occuer 2wice faster than needed - REASON: allow max time for Pin-UP, Pin-DOWN action
  Clock_Solar = 1000000/(MicroSteps_360/(SIDEREAL_DAY-235.9095));
  Clock_Lunar = 1000000/(MicroSteps_360/(SIDEREAL_DAY-2089.2292));

  analogWrite(TFTBright, 255);  // LCD Backlight brightness 255 = ~100%)
  
  rtc.begin();
  dht.begin();
  tft.begin(HX8357D);
  tft.setRotation(4);
  
  tft.fillScreen(BLACK);
  tft.setCursor(15, 10);

  // DRB8825 - drive mode pins (determine Steppping Modes 1/8, 1/16 and etc.
  pinMode(RA_MODE0, OUTPUT); 
  pinMode(RA_MODE1, OUTPUT); 
  pinMode(RA_MODE2, OUTPUT);

  pinMode(DEC_MODE0, OUTPUT); 
  pinMode(DEC_MODE1, OUTPUT); 
  pinMode(DEC_MODE2, OUTPUT);

  pinMode(RA_STP,OUTPUT); // RA Step
  pinMode(RA_DIR,OUTPUT); // RA Dir
  pinMode(DEC_STP,OUTPUT); // DEC Step
  pinMode(DEC_DIR,OUTPUT); // DEC Dir

  //digitalWrite(RA_DIR,HIGH); // Set Dir high
  //digitalWrite(RA_STP,LOW);
  //digitalWrite(DEC_DIR,HIGH); // Set Dir high
  //digitalWrite(DEC_STP,LOW);

  // Joystick
  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);

  // Device 1 & 2 command Pins
  pinMode(FAN1, OUTPUT);
  pinMode(FAN2, OUTPUT);

  // Pin to define Power to the DRV8825 using MOSFET!
  pinMode(POWER_DRV8825, OUTPUT);

  
  // Set RA and DEC microstep position 
  RA_microSteps = RA_90; //  --> point to North Sudereal Pole = -180 deg (-12h)
  DEC_microSteps = 0; //  --> Point to North Sudereal Pole = 90 deg
   
  Timer3.attachInterrupt(Sidereal_rate);
//  Timer3.start(Clock_Sidereal); // executes the code every 62.329 ms.

  
  if (analogRead(DAY_NIGHT_PIN) < 800)
  {
    texts = Maroon;
    l_text = RED;
    d_text = Maroon;
    title_bg = RED;
    title_texts = BLACK;
    messie_btn = Maroon;
    btn_l_text = RED;
    btn_d_text = Maroon;
    btn_l_border = RED;
    btn_d_border = Maroon;
    btn_l_selection = RED;
    MsgBox_bg = RED;
    MsgBox_t = BLACK;
    Button_State_ON=BLACK;
   Button_State_OFF=BLACK;
   Button_Title=BLACK;
  } else {
    IS_NIGHTMODE = false;
    texts = LightGrey;
    l_text = WHITE;
    d_text = LightGrey;
    btn_l_text = GreenYellow;
    btn_d_text = DarkGreen;
    btn_l_border = GREEN;
    btn_d_border = DarkGreen;
    btn_l_selection = DarkGreen;
    title_bg = Orange;
    title_texts = BLACK;
    messie_btn = DarkGrey;
    MsgBox_bg = Purple;
    MsgBox_t = GreenYellow;
    Button_State_ON=DarkGreen;
    Button_State_OFF=RED;
    Button_Title=Purple;
  }


  // Draw initial screen - INITIALIZE
  // The below part cannot be removed form the code
  // You can add messages, but not remove!
  tft.setTextColor(title_bg);    
  tft.setTextSize(4);
  tft.println("rDUINO SCope");
  tft.setTextColor(l_text);    
  tft.setTextSize(2);
  tft.setCursor(50, 45);
  tft.setTextColor(l_text);    
  tft.print("coded by <dEskoG>");
  tft.setCursor(40, 70);
  tft.print("Dessislav Gouzgounov");
  tft.setCursor(4, 90);
  tft.setTextSize(2);
  tft.print("rduinoscope.byethost24.com");
  tft.setCursor(6, 130);
  tft.setTextColor(d_text);    
  tft.print("GNU General Public License");
  tft.setCursor(10, 160);
  tft.setTextColor(d_text);    
  tft.println("Version: " + FirmwareNumber);
  
#ifdef serial_debug
    tft.setTextColor(title_bg);
    tft.println(" - DEBUG MODE");
    tft.setTextColor(d_text);
  #endif
  
  tft.setCursor(0, 220);
  tft.setTextSize(1);
  
  // see if the card is present and can be initialized:
  char in_char;
  String items = "";
  int j = 0;
  int k = 0;
  MESS_PAGER = 0;
  TREAS_PAGER = 0;
  STARS_PAGER = 0;

  volatile bool check = true;
  tft.print("--> Initializing touchscreen... ");
    tft.setTextColor(GREEN);
    tft.println("OK");
    tft.setTextColor(d_text);
    
  tft.print("--> Initializing DHT sensor... ");
  if(isnan(dht.readTemperature()) || isnan(dht.readHumidity()))
  {
    tft.setTextColor(RED);
    tft.println("FAIL");
    tft.setTextColor(d_text);
  }
  else
  {
    tft.setTextColor(GREEN);
    tft.println("OK");
    tft.setTextColor(d_text);
  }

  tft.print("--> Initializing RTC... ");
  int prev_mil = millis(); 
  if(isnan(rtc.getTemp()))
  {
    tft.setTextColor(RED);
    tft.println("FAIL");
    tft.setTextColor(d_text);
    check = false;
  }
  else
  {
    tft.setTextColor(GREEN);
    tft.print("OK   ");
    #ifdef serial_debug
      tft.println(rtc.getTemp());
    #else
      tft.println("");  
    #endif
    tft.setTextColor(d_text);
  }
  
  tft.print("--> Initializing SD card... ");
  for (int i=0; i<10 && !SD.begin(sd_cs); i++)
  {
    if (i == 9)
    {
      tft.setTextColor(RED);
      tft.println("ERROR: Card failed, or not present\n");
      tft.println("Try formatting the SD card to FAT32 and replace the files.");
      tft.setTextColor(d_text);
      check = false;
    }
    delay(50);
  }
  //Debug or card initialized:
  tft.setTextColor(GREEN);
  tft.println("OK");
  tft.setTextColor(d_text);
  //loadOptions_SD();
  //delay(100);
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("messier.csv");

  // if the file is available, write to it:
  if (dataFile)
  {
    tft.print("--> loading Messier catalog... ");
    delay(100);
    while (dataFile.available())
    {
      in_char = dataFile.read();
      items += in_char;
      k = k + 1;
      if (in_char == '\n') {
        Messier_Array[j] = items;
        j = j + 1;
        //          Serial.print(items);
        items = "";
      }
    }
    tft.setTextColor(GREEN);
    tft.println("OK");
    tft.setTextColor(d_text);
    delay(100);

  }
  else
  {
    tft.setTextColor(RED);
    tft.println("ERROR opening: messier.csv");
    tft.setTextColor(d_text);
  }

  dataFile.close();
  items = "";
  j = 0;
  k = 0;
  dataFile = SD.open("treasure.csv");


  // if the file is available, write to it:
  if (dataFile)
  {
    tft.print("--> loading Treasure catalog... ");
    delay(100);
    while (dataFile.available()) {
      in_char = dataFile.read();
      items += in_char;
      k = k + 1;
      if (in_char == '\n') {
        Treasure_Array[j] = items;
        j = j + 1;
        //          Serial.print(items);
        items = "";
      }
    }
    tft.setTextColor(GREEN);
    tft.println("OK");
    tft.setTextColor(d_text);
    delay(100);

  }
  else
  {
    tft.setTextColor(RED);
    tft.println("ERROR opening: treasure.csv");
    tft.setTextColor(d_text);
  }
  dataFile.close();
  last_button = 0;
  LOAD_SELECTOR = 0;

  //tft.println("\n.................................\n");
  
  tft.print("--> loading custom options...");
  if(SD.exists("options.txt"))
  {
    loadOptions_SD();
    tft.setTextColor(GREEN);
    tft.println("OK");
    tft.setTextColor(d_text);
  }
  else
  {
    tft.setTextColor(RED);
    tft.println("FAIL");
    tft.setTextColor(d_text);
  }
  tft.println("--> initializing BlueTooth");
  delay(100);
  tft.println("--> initializing GPS");

  #ifndef serial_debug
    if (check == false)  while(1);  //don't do anything more
  #endif

  calibrateJoypad(&x_cal, &y_cal);

  // Draw Supporters Logos
  tft.setCursor(0, 360);   // was 370
  tft.setTextColor(l_text);
  //tft.println("SUPPORTERS:");
  bmpDraw("Desso.bmp", 160, 330);  
  bmpDraw("hackaday.bmp", 0, 350); 
  delay(200);

  // EMPIRIAL MARCH - if sounds everything was initialized well   :)
  if (IS_SOUND_ON)
  {
    SoundOn(note_f, 48);
    delay(100);
    SoundOn(note_f, 48);
    delay(100);
    SoundOn(note_f, 48);
    delay(100);
    SoundOn(note_cb, 32);
    delay(140);
    SoundOn(note_gb, 8);
    delay(50);
    SoundOn(note_f, 48);
    delay(100);
    SoundOn(note_cb, 32);
    delay(140);
    SoundOn(note_gb, 8);
    delay(50);
    SoundOn(note_f, 48);
  }

  delay(500);
  CURRENT_SCREEN = 0;
  drawGPSScreen();
  UPD_T = millis();
  UPD_LST = millis();
  DELAY_Slew = millis();
  TFT_Timer = millis();
  //TFT_timeout = 0;
  RA_move_ending = 0;
  considerTempUpdates();

  digitalWrite(POWER_DRV8825, HIGH == !reverse_logic); // Switch on the Motor Driver Power!
}


void loop(void) {
 
  
  // This is done in order to prevent multiple calculations of LST_HA per second (especially while SlewTo) and only 
  // do it once the DEC SlewTo slows down, but before stopping OR once every 10 seconds (in order to do the Meridian Flip)
  if (RA_move_ending == 1){
     calculateLST_HA();
  }

  if ((IS_MERIDIAN_FLIP_AUTOMATIC) && ((UPD_LST + 10000) <= millis()) && (IS_OBJ_FOUND == true)){
     calculateLST_HA();
     UPD_LST = millis();
  }

 // Adding this delay to SLOW DOWN the Arduino so that the motors can catch up!
 // The delay is only needed when in full speed.... otherways the CalculateLST_HA() takes over and
 // slows down the arduino enought. CalculateLST_HA() when slewing only fires when the motors slows down
 // after they are very close to the Object Position.
 if ((DELAY_Slew + 1 <= millis()) && (IS_OBJ_FOUND == false)){
  
    // If you wonder how I get to this delay - 800 uS
    // When I optimised the code for speed, the main delay was coming from calculateLST_HA() which back then was calculated on every Loop();
    // Once I optimized it to only calculate when the SlewTo stops (to fine tune after DEC stops) it turned out that
    // the code is too fast and the motors only "screemed" but not rotating - due to the low voltage/current.
    // This variable depends on How You Limit the Current to your motors and the Voltage you use!
    // I use 12V and 1.6A (70% in full step = 1.10A) to drive my NEMA 17 SY42STH47-1684B Motors.
    // Please note that Potentiometer does not really give consistent results for current on every restart (it drifted between 1.12A - 0.9A).

    // HINT: you can try to play with the Current/Voltage that powers the mottors to get faster speeds.
    if (IS_STEPPERS_ON){
      cosiderSlewTo();
    }else{
      IS_OBJECT_RA_FOUND = true;
      IS_OBJECT_DEC_FOUND = true;
      IS_OBJ_FOUND = true;
      RA_move_ending = 0;
    }
    DELAY_Slew = millis();
    // delayMicroseconds(800);
 }

  // The below part of the code makes sure that the system does NOT process any other inputs while SlweingTo!
  // Since both motors need every STEP to come from Arduino board, it needs it's entire power to run the motors in fastest possible way
  // The fastes possible from this board in the current state of the software is approx 3 turns/sec (600 steps/sec)
  // IS_OBJ_FOUND == true --> Means that SLEW command have completed
  //
   if (IS_OBJ_FOUND == true)
  {
    // BLUETOOTH Considerations ? ... if any
    if ((IS_BT_MODE_ON == true) && (Serial3.available() > 0) && (IS_MANUAL_MOVE == false))
    {
      BT_COMMAND_STR = Serial3.readStringUntil('#');
      #ifdef serial_debug
        Serial.println(BT_COMMAND_STR);
      #endif
      considerBTCommands();
    }


      // JOYSTICK Movements ? ... if any
    xPosition = analogRead(xPin);
    yPosition = analogRead(yPin);

    if ((xPosition < x_cal-100) || (xPosition > x_cal+100) || (yPosition < y_cal-100) || (yPosition > x_cal+100))
    {
      IS_MANUAL_MOVE = true;
      if (IS_STEPPERS_ON)
      {
        consider_Manual_Move(xPosition, yPosition);
      }
    }
    else
    {
      IS_MANUAL_MOVE = false;
    }

     // This will take care of turning OFF the TFT's background light if the device is not used
    // for XXX amont of seconds and IS_IN_OPERATION = TRUE
    if ((TFT_timeout > 0) && (millis() - TFT_Timer > TFT_timeout) && (IS_TFT_ON) && (IS_IN_OPERATION))
    {
      analogWrite(TFTBright, 0);
      IS_TFT_ON = false;
    }
      
 // TOUCH SCREEN Inputs ? ... if any

    TSPoint p = ts.getPoint();  

 // Scale from ~0->1000 to tft.width using the calibration #'s
  
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
  
    int tx,ty;
       
    tx=(p.x-0);
    ty=(p.y-0);
  
  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
  
  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
     return;
  }

 //Useful to debug touch:
      #ifdef serial_debug
        Serial.print(" -> Touched: ");
        Serial.print(tx);
        Serial.print(", y = ");
        Serial.println(ty);
      #endif
    considerTouchInput(tx, ty);

          
       // OTHER UPDATES ?  ... if any
      // Happens every 2 seconds
      if (((millis()- UPD_T) > 2000)&&(IS_MANUAL_MOVE == false)){
        calculateLST_HA();  // Make sure it Updates the LST! used on Main Screen and When Calculating current Coords. 
        considerTimeUpdates();
        considerDayNightMode();
        considerTempUpdates();
        // I need to make sure the Drives are not moved to track the stars,
        // if Object is below horizon ALT < 0 - Stop tracking.
        if ((ALT <= 0) && (IS_TRACKING==true) && (IS_IN_OPERATION == true)){
            IS_TRACKING = false;
            Timer3.stop();
            drawMainScreen();
        }
        UPD_T = millis();
      }
  }
}

void calculateLST_HA(){
//  HA = LST - RA 
    String Date_q = String(rtc.getDateStr());
    String Time_q = String(rtc.getTimeStr());
    int D = Date_q.substring(0,2).toInt();
    int M = Date_q.substring(3,5).toInt();
    int Y = Date_q.substring(6).toInt();
    int S = Time_q.substring(6).toInt();
    int H = Time_q.substring(0,2).toInt(); // hours
    if (Summer_Time == 1){
      H -= 1;
    }
    int MN = Time_q.substring(3,5).toInt();
    if (M < 3) {
       M = M + 12; 
       Y = Y - 1;
    }
    
    float HH = H + ((float)MN/60.00) + ((float)S/3600.00);
    float AA= (int)(365.25*(Y+4716));
    float BB= (int)(30.6001*(M+1)); 
    double CurrentJDN = AA + BB + D - 1537.5 + (HH-TIME_ZONE)/24;
    float current_day = CurrentJDN - 2451543.5;

    //calculate terms required for LST calcuation and calculate GMST using an approximation
    double MJD = CurrentJDN - 2400000.5;    
    int MJD0 = (int)MJD;
    float ut = (MJD - MJD0)*24.0;
    double t_eph  = (MJD0-51544.5)/36525.0;     
    double GMST = 6.697374558 + 1.0027379093*ut + (8640184.812866 + (0.093104 - 0.0000062*t_eph)*t_eph)*t_eph/3600.0;   

    LST = GMST + OBSERVATION_LONGITUDE/15.0;

    //reduce it to 24 format
    int LSTint = (int)LST;
    LSTint/=24;
    LST = LST - (double) LSTint * 24;

// Now I'll use the global Variables OBJECT_RA_H and OBJECT_RA_M  To calculate the Hour angle of the selected object.
    
    double dec_RA = OBJECT_RA_M/60 + OBJECT_RA_H;

    double HA_decimal = LST - dec_RA;
    
    HAHour = int(HA_decimal);
    HAMin = (HA_decimal - HAHour)*60; 
    
    if (HAMin < 0){
      HAHour -= 1;
      HAMin +=60;
    }
    if (HAHour < 0){
      HAHour +=24;
    }
    
    // Convert degrees into Decimal Radians
    double rDEC = 0;
    rDEC = OBJECT_DEC_D + (OBJECT_DEC_M/60);
 

    //rDEC += delta_a_DEC;
    rDEC *= 0.0174532925199;
    double rHA =  HA_decimal*0.26179938779915;     // 0.261799.. = 15 * 3.1415/180  (to convert to Deg. and * Pi) :)
    double rLAT = OBSERVATION_LATTITUDE * 0.0174532925199;

    IS_OBJ_VISIBLE = true;

    double sin_rDEC = sin(rDEC);
    double cos_rDEC = cos(rDEC);
    double sin_rLAT = sin(rLAT);
    double cos_rLAT = cos(rLAT);
    double cos_rHA = cos(rHA);
    double sin_rHA = sin(rHA);
    
    ALT = sin_rDEC * sin_rLAT;
    ALT += (cos_rDEC * cos_rLAT * cos_rHA);
    double sin_rALT = ALT;
    ALT =  asin(ALT);
    double cos_rALT = cos(ALT);
    ALT *= 57.2958;
    
    AZ = sin_rALT * sin_rLAT;
    AZ = sin_rDEC - AZ;
    AZ /= (cos_rALT * cos_rLAT);
    AZ = acos(AZ)*57.2957795;
    if (sin_rHA > 0){
      AZ = 360 - AZ;
    }
    
    if (ALT < 0){
      IS_OBJ_VISIBLE = false;
      if ((IS_BT_MODE_ON == true) && (IS_OBJ_FOUND == false)){
         Serial3.println("Object is out of sight! Telescope not moved.");
      }
      IS_OBJ_FOUND = true;
      IS_OBJECT_RA_FOUND = true;
      IS_OBJECT_DEC_FOUND = true;
      Slew_RA_timer = 0;
      RA_finish_last = 0;
    }else{
      IS_OBJ_VISIBLE = true;
    }

    
    // Take care of the Meridian Flip coordinates
    // This will make the telescope do Meridian Flip... depending on the current HA and predefined parameter: MIN_TO_MERIDIAN_FLIP
    if (IS_MERIDIAN_FLIP_AUTOMATIC){
        mer_flp = HAHour + ((HAMin + MIN_TO_MERIDIAN_FLIP)/60);
        float old_HAMin = HAMin;
        float old_HAHour = HAHour;
        if (IS_POSIBLE_MERIDIAN_FLIP == true){
            if (mer_flp >= 24){
                  HAMin = HAMin - 60;
                  HAHour = 0;
                  if (MERIDIAN_FLIP_DO == false){
                      IS_TRACKING = false;
                      Timer3.stop();
                      OnScreenMsg(1);
                      if (IS_SOUND_ON){
                        SoundOn(note_C,32);
                        delay(200);
                        SoundOn(note_C,32);
                        delay(200);
                        SoundOn(note_C,32);
                        delay(1000); 
                      }           
                      IS_OBJ_FOUND = false;
                      IS_OBJECT_RA_FOUND = false;
                      IS_OBJECT_DEC_FOUND = false;
                      Slew_timer = millis();
                      Slew_RA_timer = Slew_timer + 20000;   // Give 20 sec. advance to the DEC. We will revise later.
                      MERIDIAN_FLIP_DO = true;
                      drawMainScreen();
                  }else{
                      if ((old_HAHour == HAHour) && (old_HAMin == HAMin)){   // Meridian Flip is done so the code above will not execute
                          MERIDIAN_FLIP_DO = false;
                      }
                  }
                  //DEC is set as part of the SlewTo function
              }
        }else{
            if (mer_flp >= 24){
              IS_TRACKING = false;
              Timer3.stop();
            }
        }
    }
}

void selectOBJECT_M(int index_, int objects) {
   OBJECT_Index = index_;

   if (objects == 0){                                           // I've selected a Messier Object
      int i1 = Messier_Array[index_].indexOf(';');
      int i2 = Messier_Array[index_].indexOf(';',i1+1);
      int i3 = Messier_Array[index_].indexOf(';',i2+1);
      int i4 = Messier_Array[index_].indexOf(';',i3+1);
      int i5 = Messier_Array[index_].indexOf(';',i4+1);
      int i6 = Messier_Array[index_].indexOf(';',i5+1);
      int i7 = Messier_Array[index_].indexOf(';',i6+1);
      OBJECT_NAME = Messier_Array[index_].substring(0,i1);
      OBJECT_DESCR = Messier_Array[index_].substring(i7+1,Messier_Array[index_].length()-1);
      String OBJ_RA = Messier_Array[index_].substring(i1,i2);
      OBJECT_RA_H = OBJ_RA.substring(1,OBJ_RA.indexOf('h')).toFloat();
      OBJECT_RA_M = OBJ_RA.substring(OBJ_RA.indexOf('h')+1,OBJ_RA.length()-1).toFloat();
      String OBJ_DEC = Messier_Array[index_].substring(i2,i3);
      String sign = OBJ_DEC.substring(1,2);
      OBJECT_DEC_D = OBJ_DEC.substring(2,OBJ_DEC.indexOf('°')).toFloat();
      OBJECT_DEC_M = OBJ_DEC.substring(OBJ_DEC.indexOf('°')+1,OBJ_DEC.length()-1).toFloat();
      if (sign == "-"){ 
          OBJECT_DEC_D *= -1;
          OBJECT_DEC_M *= -1;
      }
      OBJECT_DETAILS = OBJECT_NAME + " is a ";
      OBJECT_DETAILS += Messier_Array[index_].substring(i4+1,i5) + " in constelation ";
      OBJECT_DETAILS += Messier_Array[index_].substring(i3+1,i4) + ", with visible magnitude of ";
      OBJECT_DETAILS += Messier_Array[index_].substring(i5+1,i6) + " and size of ";
      OBJECT_DETAILS += Messier_Array[index_].substring(i6+1,i7);

   }else if (objects == 1){                                     // I've selected a Treasure Object
      int i1 = Treasure_Array[index_].indexOf(';');
      int i2 = Treasure_Array[index_].indexOf(';',i1+1);
      int i3 = Treasure_Array[index_].indexOf(';',i2+1);
      int i4 = Treasure_Array[index_].indexOf(';',i3+1);
      int i5 = Treasure_Array[index_].indexOf(';',i4+1);
      int i6 = Treasure_Array[index_].indexOf(';',i5+1);
      int i7 = Treasure_Array[index_].indexOf(';',i6+1);
      OBJECT_NAME = Treasure_Array[index_].substring(0,i1);
      OBJECT_DESCR = Treasure_Array[index_].substring(i7+1,Treasure_Array[index_].length()-1);
      String OBJ_RA = Treasure_Array[index_].substring(i1,i2);
      OBJECT_RA_H = OBJ_RA.substring(1,OBJ_RA.indexOf('h')).toFloat();
      OBJECT_RA_M = OBJ_RA.substring(OBJ_RA.indexOf('h')+1,OBJ_RA.length()-1).toFloat();
      String OBJ_DEC = Treasure_Array[index_].substring(i2,i3);
      String sign = OBJ_DEC.substring(1,2);
      OBJECT_DEC_D = OBJ_DEC.substring(2,OBJ_DEC.indexOf('°')).toFloat();
      OBJECT_DEC_M = OBJ_DEC.substring(OBJ_DEC.indexOf('°')+1,OBJ_DEC.length()-1).toFloat();
      if (sign == "-"){ 
          OBJECT_DEC_D *= -1;
          OBJECT_DEC_M *= -1;
      }
      OBJECT_DETAILS = OBJECT_NAME + " is a ";
      OBJECT_DETAILS += Treasure_Array[index_].substring(i4+1,i5) + " in constelation ";
      OBJECT_DETAILS += Treasure_Array[index_].substring(i3+1,i4) + ", with visible magnitude of ";
      OBJECT_DETAILS += Treasure_Array[index_].substring(i5+1,i6) + " and size of ";
      OBJECT_DETAILS += Treasure_Array[index_].substring(i6+1,i7);

   }else if (objects == 2){                                     // I'm selecting a STAR for Synchronization - 1 Star ALLIGNMENT
      int i1 = Stars[index_].indexOf(';');
      int i2 = Stars[index_].indexOf(';',i1+1);
      int i3 = Stars[index_].indexOf(';',i2+1);
      OBJECT_NAME = Stars[index_].substring(i1+1,i2)+" from "+Stars[index_].substring(0,i1);
      String OBJ_RA = Stars[index_].substring(i2+1,i3);
      OBJECT_RA_H = OBJ_RA.substring(0,OBJ_RA.indexOf('h')).toFloat();
      OBJECT_RA_M = OBJ_RA.substring(OBJ_RA.indexOf('h')+1,OBJ_RA.length()-1).toFloat();
      String OBJ_DEC = Stars[index_].substring(i3,Stars[index_].length());
      String sign = OBJ_DEC.substring(0,1);
      OBJECT_DEC_D = OBJ_DEC.substring(1,OBJ_DEC.indexOf('°')).toFloat();
      if (sign == "-"){ 
          OBJECT_DEC_D *= (-1);
      }
      OBJECT_DEC_M = 0;
   }else if (objects == 3){                                     // I'm selecting a STAR for Synchronization - Iterative ALLIGNMENT
      int i1 = Iter_Stars[index_].indexOf(';');
      int i2 = Iter_Stars[index_].indexOf(';',i1+1);
      int i3 = Iter_Stars[index_].indexOf(';',i2+1);
      OBJECT_NAME = Iter_Stars[index_].substring(i1+1,i2)+" from "+Iter_Stars[index_].substring(0,i1);
      String OBJ_RA = Iter_Stars[index_].substring(i2+1,i3);
      OBJECT_RA_H = OBJ_RA.substring(0,OBJ_RA.indexOf('h')).toFloat();
      OBJECT_RA_M = OBJ_RA.substring(OBJ_RA.indexOf('h')+1,OBJ_RA.length()-1).toFloat();
      String OBJ_DEC = Iter_Stars[index_].substring(i3,Iter_Stars[index_].length());
      String sign = OBJ_DEC.substring(0, 1);
      OBJECT_DEC_D = OBJ_DEC.substring(1,OBJ_DEC.indexOf('°')).toFloat();
      if (sign == "-"){ 
          OBJECT_DEC_D *= (-1);
      }
      OBJECT_DEC_M = 0;
   }
}

void Sidereal_rate(){
  // when a manual movement of the drive happens. - This will avoid moving the stepepers with a wrong Step Mode.
  if ((IS_MANUAL_MOVE == false) && (IS_TRACKING) && (IS_STEPPERS_ON)){
    if (RA_mode_steps != MICROSteps){
        setmStepsMode("R",MICROSteps);
    }
    digitalWrite(RA_DIR,STP_BACK);
    PIOC->PIO_SODR=(1u<<26);
    delayMicroseconds(2);
    PIOC->PIO_CODR=(1u<<26);
//    digitalWrite(RA_STP,HIGH);
    RA_microSteps += 1;
//    digitalWrite(RA_STP,LOW);
  }
}

void cosiderSlewTo(){
//  int RA_microSteps, DEC_microSteps;
//  int SLEW_RA_microsteps, SLEW_DEC_microsteps;
//  INT data type -> -2,147,483,648 to 2,147,483,647 
//  for more details see the XLS file with calculations
//...

    float HAH;
    float HAM;
    float DECD;
    float DECM;
    double HA_decimal, DEC_decimal;

    if (HAHour >= 12){
        HAH = HAHour - 12;
        HAM = HAMin;
        IS_MERIDIAN_PASSED = false;
    }else{
        HAH = HAHour;
        HAM = HAMin;
        IS_MERIDIAN_PASSED = true;
    }

    //  ADD Correction for RA && DEC according to the Star Alignment
    HA_decimal = ((HAH+(HAM/60))*15) + delta_a_RA;   // In degrees - decimal
    DEC_decimal = OBJECT_DEC_D + (OBJECT_DEC_M/60) + delta_a_DEC; //I n degrees - decimal   

    SLEW_RA_microsteps  = HA_decimal * HA_H_CONST;     // Hardware Specific Code  
    SLEW_DEC_microsteps = DEC_90 - (DEC_decimal * DEC_D_CONST);    // Hardware specific code

    if(IS_MERIDIAN_PASSED == true){
        SLEW_DEC_microsteps*= -1;
    }

// If Home Position selected .... Make sure it goes to 0.

// DO I REALLY NEED THIS.... ????
// CONSIDER THE CODE WHEN YOU HAVE TIME!!!
    int home_pos = 0;
    if ((OBJECT_RA_H == 12) && (OBJECT_RA_M == 0) && (OBJECT_DEC_D == 90) && (OBJECT_DEC_M == 0)){
        SLEW_RA_microsteps = RA_90;
        SLEW_DEC_microsteps = 0;
        home_pos = 1;
    }

// Make the motors START slow and then speed-up - using the microsteps!
// Speed goes UP in 2.2 sec....then ..... FULL Speed ..... then....Slpeed goes Down for 3/4 Revolution of the drive  
int delta_DEC_time = millis() - Slew_timer;
int delta_RA_timer = millis() - Slew_RA_timer;


    if (delta_DEC_time >= 0 && delta_DEC_time < 1800){
      
        if (DEC_mode_steps != 16){
            setmStepsMode("D",16);
        }
    }
    if (delta_DEC_time >= 1800 && delta_DEC_time < 2600){
        if (DEC_mode_steps != 8){
            setmStepsMode("D",8);
        }
    }
    if (delta_DEC_time >= 2600 && delta_DEC_time < 3400){
        if (DEC_mode_steps != 4){
            setmStepsMode("D",4);
        }
    }
    if (delta_DEC_time >= 3400){
         if (DEC_mode_steps != 2){
            setmStepsMode("D",2);
        }
    }



    if (delta_RA_timer >= 0 && delta_RA_timer < 1800){
        if (RA_mode_steps != 16){
            setmStepsMode("R",16);
        }
    }
    if (delta_RA_timer >= 1800 && delta_RA_timer < 2600){
        if (RA_mode_steps != 8){
            setmStepsMode("R",8);
        }
    }
    if (delta_RA_timer >= 2600 && delta_RA_timer < 3400){
        if (RA_mode_steps != 4){
            setmStepsMode("R",4);
        }
    }
    if (delta_RA_timer >= 3400){
        if (RA_mode_steps != 2){
            setmStepsMode("R",2);
        }
    }

    int delta_RA_steps = SLEW_RA_microsteps - RA_microSteps;
    int delta_DEC_steps = SLEW_DEC_microsteps - DEC_microSteps;

// Make the motors SLOW DOWN and then STOP - using the microsteps!
// Speed goes DOWN in 2.2 sec....then ..... FULL Speed ..... then....Speed goes Down for 3/4 Revolution of the drive  

    if ((abs(delta_DEC_steps) >= 1200) && (abs(delta_DEC_steps) <= 3000)){ 
        if (DEC_mode_steps != 8){
            setmStepsMode("D",8);
        }
    }
    if ((abs(delta_DEC_steps) < 3000)){ 
        if (DEC_mode_steps != 16){
            setmStepsMode("D",16);
        }
    }
    if ((abs(delta_RA_steps) >= 1200) && (abs(delta_RA_steps) <= 3000)){
        if (RA_mode_steps != 8){
            setmStepsMode("R",8);
        }
    }
    if (abs(delta_RA_steps) < 3000){
        if (RA_mode_steps != 16){
            setmStepsMode("R",16);
            RA_move_ending = 1;
        }
    }

    // Taking care of the RA Slew_To.... and make sure it ends Last
    // NB: This way we can jump to TRACK and be sure the RA is on target   
    if(abs(delta_RA_steps)>= abs(delta_DEC_steps)){
        if (RA_finish_last == 0){
          RA_finish_last = 1;
          Slew_RA_timer = millis();
        }
    }


// RA_STP, HIGH - PIOC->PIO_SODR=(1u<<26)
// RA_STP, LOW - PIOC->PIO_CODR=(1u<<26)
// DEC_STP, HIGH - PIOC->PIO_SODR=(1u<<24)
// DEC_STP, LOW - PIOC->PIO_CODR=(1u<<24)

    if ((IS_OBJECT_RA_FOUND == false) && (RA_finish_last == 1)){    
            if (SLEW_RA_microsteps >= (RA_microSteps-RA_mode_steps) && SLEW_RA_microsteps <= (RA_microSteps+RA_mode_steps)){
               IS_OBJECT_RA_FOUND = true;               
            } else {
                  if (SLEW_RA_microsteps > RA_microSteps){
                      digitalWrite(RA_DIR,STP_BACK);
                      //digitalWrite(RA_STP,HIGH);
                      //digitalWrite(RA_STP,LOW);
                      PIOC->PIO_SODR=(1u<<26);
                      delayMicroseconds(5);
                      PIOC->PIO_CODR=(1u<<26);
                      RA_microSteps += RA_mode_steps;
                  }else{
                      digitalWrite(RA_DIR,STP_FWD);
                      //digitalWrite(RA_STP,HIGH);
                      //digitalWrite(RA_STP,LOW);
                      PIOC->PIO_SODR=(1u<<26);
                      delayMicroseconds(5);
                      PIOC->PIO_CODR=(1u<<26);
                      RA_microSteps -= RA_mode_steps;
                  } 
            }
    }

    // Taking care of the DEC Slew_To....  
    if (IS_OBJECT_DEC_FOUND == false){    
        if (SLEW_DEC_microsteps >= (DEC_microSteps-DEC_mode_steps) && SLEW_DEC_microsteps <= (DEC_microSteps+DEC_mode_steps)){
              IS_OBJECT_DEC_FOUND = true;
        } else {
              if (SLEW_DEC_microsteps > DEC_microSteps){
                  digitalWrite(DEC_DIR,STP_BACK);
                  //digitalWrite(DEC_STP,HIGH);
                  //digitalWrite(DEC_STP,LOW);
                  PIOC->PIO_SODR=(1u<<24);
                  delayMicroseconds(5);
                  PIOC->PIO_CODR=(1u<<24);
                  DEC_microSteps += DEC_mode_steps;
              }else{
                  digitalWrite(DEC_DIR,STP_FWD);
                  //digitalWrite(DEC_STP,HIGH);
                  //digitalWrite(DEC_STP,LOW);
                  PIOC->PIO_SODR=(1u<<24);
                  delayMicroseconds(5);
                  PIOC->PIO_CODR=(1u<<24);
                  DEC_microSteps -= DEC_mode_steps;
              } 
        }
    }

    // Check if Object is found on both Axes...
    if (IS_OBJECT_RA_FOUND == true && IS_OBJECT_DEC_FOUND == true){
      IS_OBJ_FOUND = true;
      RA_move_ending = 0;

      if ((home_pos == 0 ) && (ALT > 0)){
          IS_TRACKING = true;
          setmStepsMode("R",MICROSteps);
          if (Tracking_type == 1){  // 1: Sidereal, 2: Solar, 0: Lunar;
              Timer3.start(Clock_Sidereal);
          }else if (Tracking_type == 2){
              Timer3.start(Clock_Solar);
          }else if (Tracking_type == 0){
              Timer3.start(Clock_Lunar);            
          }
      }
      if (IS_SOUND_ON){
       SoundOn(note_C,64);
      }
      Slew_RA_timer = 0;
      RA_finish_last = 0;
      if (IS_BT_MODE_ON == true){
         Serial3.println("Slew done! Object in scope!");
      }
      if (IS_IN_OPERATION == true){
        drawMainScreen();
      }else{
        drawConstelationScreen(SELECTED_STAR);
      }
    }
}

void consider_Manual_Move(int xP, int yP) {
  if ((xP > 0) && (xP <= 150)) {
    setmStepsMode("R", 1);
    digitalWrite(RA_DIR, STP_BACK);
    digitalWrite(RA_STP, HIGH);
    digitalWrite(RA_STP, LOW);
    RA_microSteps += RA_mode_steps;
  } else if ((xP > 150) && (xP <= 320)) {
    setmStepsMode("R", 4);
    digitalWrite(RA_DIR, STP_BACK);
    digitalWrite(RA_STP, HIGH);
    digitalWrite(RA_STP, LOW);
    RA_microSteps += RA_mode_steps;
  } else if ((xP > 320) && (xP <= 470)) {
    setmStepsMode("R", 8);
    digitalWrite(RA_DIR, STP_BACK);
    digitalWrite(RA_STP, HIGH);
    digitalWrite(RA_STP, LOW);
    RA_microSteps += RA_mode_steps;
  } else if ((xP > 620) && (xP <= 770)) {
    setmStepsMode("R", 8);
    digitalWrite(RA_DIR, STP_FWD);
    digitalWrite(RA_STP, HIGH);
    digitalWrite(RA_STP, LOW);
    RA_microSteps -= RA_mode_steps;
  } else if ((xP > 770) && (xP <= 870)) {
    setmStepsMode("R", 4);
    digitalWrite(RA_DIR, STP_FWD);
    digitalWrite(RA_STP, HIGH);
    digitalWrite(RA_STP, LOW);
    RA_microSteps -= RA_mode_steps;
  } else if ((xP > 870) && (xP <= 1023)) {
    setmStepsMode("R", 1);
    digitalWrite(RA_DIR, STP_FWD);
    digitalWrite(RA_STP, HIGH);
    digitalWrite(RA_STP, LOW);
    RA_microSteps -= RA_mode_steps;
  }

  if ((yP > 0) && (yP <= 150)) {
    setmStepsMode("D", 1);
    digitalWrite(DEC_DIR, STP_BACK);
    digitalWrite(DEC_STP, HIGH);
    digitalWrite(DEC_STP, LOW);
    DEC_microSteps += DEC_mode_steps;
  } else if ((yP > 150) && (yP <= 320)) {
    setmStepsMode("D", 4);
    digitalWrite(DEC_DIR, STP_BACK);
    digitalWrite(DEC_STP, HIGH);
    digitalWrite(DEC_STP, LOW);
    DEC_microSteps += DEC_mode_steps;
  } else if ((yP > 320) && (yP <= 470)) {
    setmStepsMode("D", 8);
    digitalWrite(DEC_DIR, STP_BACK);
    digitalWrite(DEC_STP, HIGH);
    digitalWrite(DEC_STP, LOW);
    DEC_microSteps += DEC_mode_steps;
  } else if ((yP > 620) && (yP <= 770)) {
    setmStepsMode("D", 8);
    digitalWrite(DEC_DIR, STP_FWD);
    digitalWrite(DEC_STP, HIGH);
    digitalWrite(DEC_STP, LOW);
    DEC_microSteps -= DEC_mode_steps;
  } else if ((yP > 770) && (yP <= 870)) {
    setmStepsMode("D", 4);
    digitalWrite(DEC_DIR, STP_FWD);
    digitalWrite(DEC_STP, HIGH);
    digitalWrite(DEC_STP, LOW);
    DEC_microSteps -= DEC_mode_steps;
  } else if ((yP > 870) && (yP <= 1023)) {
    setmStepsMode("D", 1);
    digitalWrite(DEC_DIR, STP_FWD);
    digitalWrite(DEC_STP, HIGH);
    digitalWrite(DEC_STP, LOW);
    DEC_microSteps -= DEC_mode_steps;
  }
  delayMicroseconds(1500);
}

// Keep the GPS sensor "fed" until we find the data.
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  if (IS_SOUND_ON) {
    SoundOn(note_c, 8);
  }
  do
  {
    while (Serial2.available())
      gps.encode(Serial2.read());
  } while (millis() - start < ms);
}

void setmStepsMode(char* P, int mod){    
// P means the axis: RA or DEC; mod means MicroSteppping mode: x32, x16, x8....
// setmStepsMode(R,2) - means RA with 1/2 steps; setmStepsMode(R,4) - means RA with 1/4 steps


// PINS Mapping for fast switching
// DEC_M2 - Pin 8 UP - PC22 - PIOC->PIO_SODR=(1u<<22);
// DEC_M1 - Pin 9 UP - PC21 -  PIOC->PIO_SODR=(1u<<21);
// DEC_M0 - Pin 10 UP - PC29 -  PIOC->PIO_SODR=(1u<<29);
// RA_M0 - Pin 11 UP - PD7 -  PIOD->PIO_SODR=(1u<<7);
// RA_M1 - Pin 12 UP - PD8 -  PIOD->PIO_SODR=(1u<<8);
// RA_M2 - Pin 13 UP - PB27 -  PIOB->PIO_SODR=(1u<<27);
// DEC_M2 - Pin 8 DOWN - PC22 - PIOC->PIO_CODR=(1u<<22);
// DEC_M1 - Pin 9 DOWN - PC21 -  PIOC->PIO_CODR=(1u<<21);
// DEC_M0 - Pin 10 DOWN - PC29 -  PIOC->PIO_CODR=(1u<<29);
// RA_M0 - Pin 11 DOWN - PD7 -  PIOD->PIO_CODR=(1u<<7);
// RA_M1 - Pin 12 DOWN - PD8 -  PIOD->PIO_CODR=(1u<<8);
// RA_M2 - Pin 13 DOWN - PB27 -  PIOB->PIO_CODR=(1u<<27);
//
// PIOC->PIO_SODR=(1u<<25); // Set Pin High
// PIOC->PIO_CODR=(1u<<25); // Set Pin Low

  if (P=="R"){  // Set RA modes
    if (mod == 1){                      // Full Step
        //digitalWrite(RA_MODE0, LOW);
        //digitalWrite(RA_MODE1, LOW);
        //digitalWrite(RA_MODE2, LOW);
        PIOD->PIO_CODR=(1u<<7);
        PIOD->PIO_CODR=(1u<<8);
        PIOB->PIO_CODR=(1u<<27);
    }
    if (mod == 2){                      // 1/2 Step
        //digitalWrite(RA_MODE0, HIGH);
        //digitalWrite(RA_MODE1, LOW);
        //digitalWrite(RA_MODE2, LOW);
        PIOD->PIO_SODR=(1u<<7);
        PIOD->PIO_CODR=(1u<<8);
        PIOB->PIO_CODR=(1u<<27);
    }
    if (mod == 4){                      // 1/4 Step
        //digitalWrite(RA_MODE0, LOW);
        //digitalWrite(RA_MODE1, HIGH);
        //digitalWrite(RA_MODE2, LOW);
        PIOD->PIO_CODR=(1u<<7);
        PIOD->PIO_SODR=(1u<<8);
        PIOB->PIO_CODR=(1u<<27);
    }
    if (mod == 8){                      // 1/8 Step
        //digitalWrite(RA_MODE0, HIGH);
        //digitalWrite(RA_MODE1, HIGH);
        //digitalWrite(RA_MODE2, LOW);
        PIOD->PIO_SODR=(1u<<7);
        PIOD->PIO_SODR=(1u<<8);
        PIOB->PIO_CODR=(1u<<27);
    }
    if (mod == 16){                      // 1/16 Step
        //digitalWrite(RA_MODE0, LOW);
        //digitalWrite(RA_MODE1, LOW);
        //digitalWrite(RA_MODE2, HIGH);
        PIOD->PIO_CODR=(1u<<7);
        PIOD->PIO_CODR=(1u<<8);
        PIOB->PIO_SODR=(1u<<27);
    }
    if (mod == 32){                      // 1/32 Step
        //digitalWrite(RA_MODE0, HIGH);
        //digitalWrite(RA_MODE1, LOW);
        //digitalWrite(RA_MODE2, HIGH);
        PIOD->PIO_SODR=(1u<<7);
        PIOD->PIO_CODR=(1u<<8);
        PIOB->PIO_SODR=(1u<<27);
    }
    RA_mode_steps = MICROSteps/mod;
  }
  if (P=="D"){  // Set RA modes
    if (mod == 1){                      // Full Step
        //digitalWrite(DEC_MODE0, LOW);
        //digitalWrite(DEC_MODE1, LOW);
        //digitalWrite(DEC_MODE2, LOW);
        PIOC->PIO_CODR=(1u<<29);
        PIOC->PIO_CODR=(1u<<21);
        PIOC->PIO_CODR=(1u<<22);
    }
    if (mod == 2){                      // 1/2 Step
        //digitalWrite(DEC_MODE0, HIGH);
        //digitalWrite(DEC_MODE1, LOW);
        //digitalWrite(DEC_MODE2, LOW);
        PIOC->PIO_SODR=(1u<<29);
        PIOC->PIO_CODR=(1u<<21);
        PIOC->PIO_CODR=(1u<<22);
    }
    if (mod == 4){                      // 1/4 Step
        //digitalWrite(DEC_MODE0, LOW);
        //digitalWrite(DEC_MODE1, HIGH);
        //digitalWrite(DEC_MODE2, LOW);
        PIOC->PIO_CODR=(1u<<29);
        PIOC->PIO_SODR=(1u<<21);
        PIOC->PIO_CODR=(1u<<22);
    }
    if (mod == 8){                      // 1/8 Step
        //digitalWrite(DEC_MODE0, HIGH);
        //digitalWrite(DEC_MODE1, HIGH);
        //digitalWrite(DEC_MODE2, LOW);
        PIOC->PIO_SODR=(1u<<29);
        PIOC->PIO_SODR=(1u<<21);
        PIOC->PIO_CODR=(1u<<22);
    }
    if (mod == 16){                      // 1/16 Step
        //digitalWrite(DEC_MODE0, LOW);
        //digitalWrite(DEC_MODE1, LOW);
        //digitalWrite(DEC_MODE2, HIGH);
        PIOC->PIO_CODR=(1u<<29);
        PIOC->PIO_CODR=(1u<<21);
        PIOC->PIO_SODR=(1u<<22);
    }
    if (mod == 32){                      // 1/32 Step
        //digitalWrite(DEC_MODE0, HIGH);
        //digitalWrite(DEC_MODE1, LOW);
        //digitalWrite(DEC_MODE2, HIGH);
        PIOC->PIO_SODR=(1u<<29);
        PIOC->PIO_CODR=(1u<<21);
        PIOC->PIO_SODR=(1u<<22);
    }
    DEC_mode_steps = MICROSteps/mod;
  }
  delayMicroseconds(5);   // Makes sure the DRV8825 can follow
}

void SoundOn(int note, int duration){
  duration *= 10000;
  long elapsed_time = 0;
  while (elapsed_time < duration) {
      digitalWrite(speakerOut,HIGH);
      delayMicroseconds(note / 2);
      // DOWN
      digitalWrite(speakerOut, LOW);
      delayMicroseconds(note / 2);
      // Keep track of how long we pulsed
      elapsed_time += (note);
  } 
}

void UpdateObservedObjects(){
   // Write down the Observed objects information: --- USED in the STATS screen and sent to BT as status.
   int Delta_Time = (((String(rtc.getTimeStr()).substring(0,2).toInt())*60)  + (String(rtc.getTimeStr()).substring(3,5).toInt())) - ((Prev_Obj_Start.substring(0,2).toInt() * 60) + Prev_Obj_Start.substring(3).toInt());
   if (Delta_Time < 0){
      Delta_Time += 1440;
   }
   ObservedObjects[Observed_Obj_Count-1]+= ";"+String(Delta_Time);
   ObservedObjects[Observed_Obj_Count]= OBJECT_NAME+";"+OBJECT_DETAILS+";"+String(rtc.getTimeStr()).substring(0,5)+";"+int(_temp)+";"+int(_humid)+";"+int(HAHour)+"h "+HAMin+"m;"+int(ALT);
   Observed_Obj_Count +=1;
   Prev_Obj_Start = String(rtc.getTimeStr()).substring(0,5);
}

void Current_RA_DEC(){
      //curr_RA_H, curr_RA_M, curr_RA_S, curr_DEC_D, curr_DEC_M, curr_DEC_S;
      // curr_RA_lz, curr_DEC_lz, curr_HA_lz;
      // DEC

      // To ALSO correct for the Star Alignment offset
      float tmp_dec = (float(DEC_90) - float(abs(DEC_microSteps))) / float(DEC_D_CONST);
      tmp_dec -= delta_a_DEC;
      int sDEC_tel = 0;
      if (tmp_dec < 0){
         sDEC_tel = 45;
      }else{
         sDEC_tel = 43;
      }
      if (tmp_dec > 0){
        curr_DEC_D = floor(tmp_dec);
      }else{
        curr_DEC_D = ceil(tmp_dec);        
      }
      curr_DEC_M = (tmp_dec - floor(curr_DEC_D))*60;
      curr_DEC_S = (curr_DEC_M - floor(curr_DEC_M))*60;
        
      sprintf(curr_DEC_lz, "%c%02d%c%02d:%02d", sDEC_tel, int(abs(curr_DEC_D)), 223, int(abs(curr_DEC_M)), int(curr_DEC_S));

      // HOUR ANGLE
      // To correct for the Star Alignment
      double tmp_ha = double(RA_microSteps)/double(HA_H_CONST);
      tmp_ha -= delta_a_RA; 
      if (DEC_microSteps > 0){
          tmp_ha +=180;
      }
      tmp_ha /= 15;

      float tmp_ha_h = 0;
      float tmp_ha_m = 0;
      float tmp_ha_s = 0;
      tmp_ha_h = floor(tmp_ha);
      tmp_ha_m = (tmp_ha - floor(tmp_ha))*60;
      tmp_ha_s = (tmp_ha_m - floor(tmp_ha_m))*60;
      sprintf(curr_HA_lz, "%02d:%02d:%02d", int(tmp_ha_h), int(tmp_ha_m), int(tmp_ha_s));

      // RIGHT ASC.
      double tmp_ra = LST - tmp_ha; 
      if (LST < tmp_ha){
        tmp_ra += 24;
      }
      
      float tmp_ra_h = 0;
      float tmp_ra_m = 0;
      float tmp_ra_s = 0;
      curr_RA_H = floor(tmp_ra);
      curr_RA_M = (tmp_ra - curr_RA_H)*60;
      curr_RA_S = (curr_RA_M - floor(curr_RA_M))*60;
      sprintf(curr_RA_lz, "%02d:%02d:%02d", int(curr_RA_H), int(curr_RA_M), int(curr_RA_S));
}

void DrawButton(int X, int Y, int Width, int Height, String Caption, int16_t BodyColor, int16_t BorderColor, int16_t TextColor, int tSize){
//  TYPE: 0:Solid color, no Frame; 1: Frame Only button; 2: Solid color and Frame button;

    if ((BodyColor != 0)&&(BorderColor == 0)){
        // Button Type = 0 ... Solid color, no Frame      
        tft.fillRect(X, Y, Width, Height, BodyColor);
    }else if ((BodyColor == 0)&&(BorderColor != 0)){
        // Button Type = 1 ... Frame Only button      
        tft.drawRect(X, Y, Width, Height, BorderColor);
        tft.fillRect(X+1, Y+1, Width-2, Height-2, BLACK);
    }else if ((BodyColor != 0)&&(BorderColor != 0)){
        // Button Type = 1 ... Frame Only button      
        tft.drawRect(X, Y, Width, Height, BorderColor);
        tft.fillRect(X+1, Y+1, Width-2, Height-2, BodyColor);
    }else{
        // Will not Draw Button and will return to code!
        return;
    }
   
   float TX = 0;
   float TY = 0;
   if (tSize == 2){       // 10 x 14 px. (W x H)
      TX = (X + 1 + Width/2) - (Caption.length() * 6);
      TY = Y + Height/2 - 5;
   }else if (tSize == 1){ // 5 x 7 px. (W x H)
      TX = (X + 1 + Width/2) - (Caption.length() * 3);
      TY = Y + Height/2 - 3 ;
   }else if (tSize == 3){ // 15 x 21 px. (W x H)
      TX = (X + 1 + Width/2) - (Caption.length() * 8);
      TY = Y + Height/2 - 10;
   }
   tft.setCursor((int)TX, (int)TY);
   tft.setTextSize(tSize);
   tft.setTextColor(TextColor);
   if (Caption == "+"){
      TX -=5;
      tft.drawLine((int)TX+10, (int)TY-5, (int)TX + 10, (int)TY-5, TextColor);
      tft.drawLine((int)TX+8, (int)TY-4, (int)TX + 12, (int)TY-4, TextColor);
      tft.drawLine((int)TX+6, (int)TY-3, (int)TX + 14, (int)TY-3, TextColor);
      tft.drawLine((int)TX+4, (int)TY-2, (int)TX + 16, (int)TY-2, TextColor);
      tft.drawLine((int)TX+2, (int)TY-1, (int)TX + 18, (int)TY-1, TextColor);
      tft.drawLine((int)TX, (int)TY,(int)TX + 20, (int)TY, TextColor);
      tft.drawLine((int)TX-2, (int)TY+1,(int)TX + 22, (int)TY+1, TextColor);
      tft.drawLine((int)TX-4, (int)TY+2,(int)TX + 24, (int)TY+2, TextColor);

      tft.drawLine((int)TX-4, (int)TY+8,(int)TX + 24, (int)TY+8, TextColor);
      tft.drawLine((int)TX-2, (int)TY+9,(int)TX + 22, (int)TY+9, TextColor);
      tft.drawLine((int)TX, (int)TY+10,(int)TX + 20, (int)TY+10, TextColor);
      tft.drawLine((int)TX+2, (int)TY+11,(int)TX + 18, (int)TY+11, TextColor);
      tft.drawLine((int)TX+4, (int)TY+12,(int)TX + 16, (int)TY+12, TextColor);
      tft.drawLine((int)TX+6, (int)TY+13,(int)TX + 14, (int)TY+13, TextColor);
      tft.drawLine((int)TX+8, (int)TY+14,(int)TX + 12, (int)TY+14, TextColor);
      tft.drawLine((int)TX+10, (int)TY+15,(int)TX + 10, (int)TY+15, TextColor);
   }else{
      tft.println(Caption);
   }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BUFFPIXEL 85         // the Maximum number is 85

void bmpDraw(char *filename, uint8_t x, uint16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    return;
  }
  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    
    Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    
    // Read DIB header
    Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
    bmpDepth = read16(bmpFile); // bits per pixel
    
    if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
    goodBmp = true; // Supported BMP format -- proceed!
    // BMP rows are padded (if needed) to 4-byte boundary
    rowSize = (bmpWidth * 3 + 3) & ~3;

    // If bmpHeight is negative, image is in top-down order.
    // This is not canon but has been observed in the wild.
    if(bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip      = false;
     }
        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

        if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
          pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
      } // end goodBmp
    }
  }
  bmpFile.close();
 }

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

/*
 * isSummerTime() : uses Time.h
 * ---------
 * 
 * This function versifies if the date time needs to be updated by adding 1h due to summer-time
 * 
 * Return: true if in summer-time, false otherwise
*/

bool isSummerTime() //in Italy: Summer time ends the last sunday of october and begins the last of march
{
  bool summer_time = false;

  #ifdef serial_debug
    Serial.print(day());
    Serial.print("/");
    Serial.print(month());
    Serial.print("/");
    Serial.print(year());
    Serial.print("  weekday: ");
    Serial.println(weekday());
    Serial.print(hour());
    Serial.print(":");
    Serial.print(minute());
  #endif

  // If I'm in October
    if (month() == 10)
    {
        // If it's Sunday
        if (weekday() == 1)
        {
            if (day() + 7 > 31 && hour() >= 3) summer_time = false;
            else summer_time = true;
        }
        else
        {
            // If last sunday has passed
            if ((day() + 7 - (weekday() - 1)) > 31) summer_time = false;
            else  summer_time = true;
        }
    }
    // If I'm in March
    else if (month() == 3)
    {
        // If It's Sunday
        if (weekday() == 1)
        {
            // If It is the Last Sunday
            if ((day() + 7) > 31 && hour() >= 2) summer_time = true;
            else summer_time = false;
        }
        else
        {
            // If it's not Sunday,but the last has already passed
            if ((day() + 7 - (weekday() - 1)) > 31) summer_time = true;
            else  summer_time = false;
        }
    }
    // If the Day Light Saving Time
    else if (month() >= 4 && month() <= 9) summer_time = true;
    // If we are in the Winter Time Period
    else if ((month() >= 1 && month() <= 2) || (month() >= 11 && month() <= 12)) summer_time = false;
    
    return summer_time; 
}

// Performs ~310.000 readings to find the mean value of the joypad (error: 3*10^-4 % )
void calibrateJoypad(int *x_cal, int *y_cal)
{
  tft.println("\n");
  tft.setTextColor(title_bg);
  tft.println("Please avoid touching the Joypad for the next 3 seconds to let the software calibrate.\n");
  tft.setTextColor(l_text);

  int now_t = millis();
  int prev_t = millis();
  int c = 3;

  tft.print("3.. ");
  while(millis() < now_t + 3000)
  {
    if(millis() > prev_t + 1000)
    {
      c--;
      tft.print(c);
      tft.print(".. ");
      prev_t = millis();
    }
    *x_cal = (*x_cal + analogRead(A0)) / 2;
    *y_cal = (*y_cal + analogRead(A1)) / 2;
  }

  tft.setTextColor(GREEN);
  tft.print("done!");
  tft.setTextColor(l_text);
  #ifdef serial_debug
    Serial.print("x_cal: ");
    Serial.println(*x_cal);
    Serial.print("y_cal: ");
    Serial.println(*y_cal);
  #endif
}

void storeOptions_SD()
{
  SD.remove("options.txt");
  File dataFile = SD.open("options.txt", FILE_WRITE);

  if (dataFile)
  {
    String options =                       "(" +
      (String)TFT_Brightness             + ";" +
              TFT_Time                   + ";" + 
      (String)Tracking_type              + ";" + 
      (String)IS_MERIDIAN_FLIP_AUTOMATIC + ";" + 
              Fan1_State                 + ";" +
              Fan2_State                 + ";" +
      (String)IS_SOUND_ON                + ";" +
      (String)IS_STEPPERS_ON             + ")" ;

    dataFile.print(options);
    dataFile.close();
  }
}

void loadOptions_SD()
{
  File dataFile = SD.open("options.txt");

  if (dataFile)
  {
    char optionsBuffer[100] = {""};
    dataFile.read(optionsBuffer, 100);
    String receivedString = optionsBuffer;
    
    #ifdef serial_debug  
      Serial.print("options:");
      Serial.println(optionsBuffer);
    #endif

    int iniPac = receivedString.indexOf('(');
    int endPac = receivedString.indexOf(')');

    if (iniPac != -1 && endPac != -1 && endPac - iniPac > 1)
    {
      String packetIn = optionsBuffer;
      packetIn = packetIn.substring(1, endPac); //tolgo le parentesi
      String valoriIn[8] = {""};

      #ifdef serial_debug
        Serial.print("packetIn:");
        Serial.println(packetIn);
      #endif

      for (int i=0; i<sizeof(valoriIn)/sizeof(valoriIn[0])-1; i++)  //genero le sottostringhe
      {
        int endVal  = packetIn.indexOf(";");
        valoriIn[i] = packetIn.substring(0, endVal);
        #ifdef serial_debug
          Serial.print("valoriIn[");
          Serial.print(i);
          Serial.print("] :");
          Serial.println(valoriIn[i]);
        #endif
        packetIn    = packetIn.substring(endVal+1, packetIn.length());
      }
      valoriIn[sizeof(valoriIn)/sizeof(valoriIn[0])-1] = packetIn;

      //Update:
      TFT_Brightness             = valoriIn[0].toInt();          
      TFT_Time                   = valoriIn[1];
      Tracking_type              = valoriIn[2].toInt();
      IS_MERIDIAN_FLIP_AUTOMATIC = valoriIn[3].toInt(); 
      Fan1_State                 = valoriIn[4];
      Fan2_State                 = valoriIn[5];
      IS_SOUND_ON                = valoriIn[6].toInt();
      IS_STEPPERS_ON             = valoriIn[7].toInt();

      analogWrite(TFTBright, TFT_Brightness);

      if (Tracking_type == 0) Tracking_Mode = "Lunar";
      else if (Tracking_type == 2) Tracking_Mode = "Solar";
      else
      {
        Tracking_type = 1;
        Tracking_Mode = "Celest";
      }

      if      (TFT_Time.equals("30 s"  )) TFT_timeout = 30000;
      else if (TFT_Time.equals("60 s"  )) TFT_timeout = 60000;
      else if (TFT_Time.equals("2 min" )) TFT_timeout = 120000;
      else if (TFT_Time.equals("5 min" )) TFT_timeout = 300000;
      else if (TFT_Time.equals("10 min")) TFT_timeout = 600000;
      else
      {
        TFT_Time = "AL-ON";
        TFT_timeout = 0;
      }

      #ifdef serial_debug
        Serial.print("TFT_timeout = ");
        Serial.println(TFT_timeout);
      #endif

      if (IS_MERIDIAN_FLIP_AUTOMATIC) Mer_Flip_State = "AUTO";
      else
      {
        IS_MERIDIAN_FLIP_AUTOMATIC = false;
        Mer_Flip_State = "OFF";
      }

      if(Fan1_State.equals("OFF"))
        IS_FAN1_ON = false;
      else
      {
        Fan1_State = "ON";
        IS_FAN1_ON = true;
      }
      if(Fan2_State.equals("OFF"))
        IS_FAN2_ON = false;
      else
      {
        Fan2_State = "ON";
        IS_FAN2_ON = true;
      }

      if(IS_SOUND_ON)
      {
        Sound_State = "ON";
      }
      else  
      {
        IS_SOUND_ON = false;
        Sound_State = "OFF";
      }   
                              
      if(IS_STEPPERS_ON)
        Stepper_State = "ON";
      else  
      {
        IS_STEPPERS_ON = false;
        Stepper_State = "OFF";
      }
    }
    
    dataFile.close();
  }
}
