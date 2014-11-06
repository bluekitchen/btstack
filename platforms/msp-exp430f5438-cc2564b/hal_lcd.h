/*******************************************************************************
    Filename: hal_lcd.h

    Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#ifndef HAL_LCD_H
#define HAL_LCD_H

#ifndef MIN
#define MIN(n,m)   (((n) < (m)) ? (n) : (m))
#endif

#ifndef MAX
#define MAX(n,m)   (((n) < (m)) ? (m) : (n))
#endif

#ifndef ABS
#define ABS(n)     (((n) < 0) ? -(n) : (n))
#endif

#define LCD_COMM_OUT        P8OUT
#define LCD_COMM_DIR        P8DIR
#define LCD_COMM_SEL        P8SEL
#define LCD_BACKLIGHT_PIN   BIT3

#ifdef REV_02
  #define LCD_CS_RST_DIR      LCD_COMM_DIR
  #define LCD_CS_RST_OUT      LCD_COMM_OUT
  #define LCD_CS_PIN          BIT1
  #define LCD_RESET_PIN       BIT2
#else 
  #define LCD_CS_RST_DIR      P9DIR
  #define LCD_CS_RST_OUT      P9OUT  
  #define LCD_CS_PIN          BIT6 
  #define LCD_RESET_PIN       BIT7
#endif 

#define LCD_ROW                 110
#define LCD_COL                 138
#define LCD_Size                3505
#define LCD_MEM_Size            110*17
#define LCD_Max_Column_Offset   0x10  
 
#define LCD_Last_Pixel          3505

#define LCD_MEM_Row             0x11
#define LCD_Row                 0x20

// Grayscale level definitions
#define PIXEL_OFF               0
#define PIXEL_LIGHT             1
#define PIXEL_DARK              2
#define PIXEL_ON                3

#define INVERT_TEXT             BIT0
#define OVERWRITE_TEXT          BIT2

/*-------------------------------------------------------------
 *                  Function Prototypes 
 * ------------------------------------------------------------*/ 
void halLcdInit(void);                   
void halLcdShutDown(void);
void halLcdBackLightInit(void);
void halLcdSetBackLight(unsigned char BackLightLevel);
unsigned int halLcdGetBackLight(void);
void halLcdShutDownBackLight(void);
void halLcdSendCommand(unsigned char Data[]) ;
void halLcdSetContrast(unsigned char ContrastLevel);
unsigned char halLcdGetContrast(void);
void halLcdStandby(void);
void halLcdActive(void);

//Move to specified LCD address
void halLcdSetAddress(int Address);          

//Draw at current segment location
void halLcdDrawCurrentBlock(unsigned int Value);           

//Draw at specified location by calling
//LCD_Set_Address(Address) & LCD_Draw_Current_Block( value )
void halLcdDrawBlock(unsigned int Address, unsigned int Value); 

//Read value from LCD CGRAM
int halLcdReadBlock(unsigned int Address);

//Clear LCD Screen  
void halLcdClearScreen(void);                    

//Invert black to white and vice versa
void halLcdReverse(void);

// Draw a Pixel @ (x,y) with GrayScale level
void halLcdPixel(  int x,  int y, unsigned char GrayScale);
//Draw Line from (x1,y1) to (x2,y2) with GrayScale level
void halLcdLine(  int x1,  int y1,  int x2,  int y2, unsigned char GrayScale); 

void halLcdCircle(int x, int y, int Radius, int GrayScale);

void halLcdImage(const unsigned int Image[], int Columns, int Rows, int x, int y);
void halLcdClearImage(int Columns, int Rows,  int x, int y);

//Print String of Length starting at current LCD location
void halLcdPrint( char String[], unsigned char TextStyle) ;

//Print String of Length starting at (x,y)
void halLcdPrintXY(char String[], int x, int y, unsigned char TextStyle);  

//Print String of Length starting at (x,y)
void halLcdPrintLine(char String[], unsigned char Line, unsigned char TextStyle);  
void halLcdPrintLineCol(char String[], unsigned char Line, unsigned char Col, unsigned char TextStyle);  

void halLcdCursor(void);
void halLcdCursorOff(void);
//Scroll a single row of pixels
void halLcdScrollRow(int y);
//Scroll a number of consecutive rows from yStart to yEnd
void halLcdHScroll(int yStart, int yEnd);
//Scroll a line of text
void halLcdScrollLine(int Line);

#endif
