// set the correct pin connections for the WT588D chip
#define WT588D_RST 4  //Module pin "REST" or pin # 1
#define WT588D_CS 7   //Module pin "P02" or pin # 11
#define WT588D_SCL 5  //Module pin "P03" or pin # 10
#define WT588D_SDA 6  //Module pin "P01" or pin # 12
#define WT588D_BUSY 8 //Module pin "LED/BUSY" or pin # 15

// define playlistnumbers for convenience
// these playlistnumbers have been set in the flashing tool "WT588D VoiceChip Beta 1.6.exe"
// and downloaded into the flash memory
#define SCHWERT_AUSFAHREN 0
#define SCHWERT_EINFAHREN 1
#define BRUMMEN_BEWEGEN 2
#define NUR_BRUMMEN 3
#define BRUMMEN_LANGSAM 4
#define BRUMMEN_SCHNELL 5
#define BRUMMEN_SCHLAG 6
#define SCHLAG 7
#define SCHLAG_2 8
#define SCHWERT_EIN_2 9

#define SCHALTERPIN 12
#define LEDPOWERPIN A0 

#define MILLISKURZ 1000
#define MILLISLANG 6000
#define MILLISGANZLANG 9000

#define SCHWERT_VERZOEGERUNG 40

#define ANZAHLSPEZIALZUSTAENDE 2

// die ersten fünf sind standard Zustände des Laserschwertes, die folgenden Extras.
enum laserSchwertStatus_enum  {ausfahren,  istEin, einfahren, istAus, treffer, bewegen, ruhen, spezialGluehen, spezialKnightRider};
typedef enum laserSchwertStatus_enum laserSchwertModus;

enum ledSignal_enum  {an, aus, langsamBlinken,  schnellBlinken, zweimalSchnell};
typedef enum ledSignal_enum ledSignalModus;

