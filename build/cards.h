#ifndef CARDS_H   
#define CARDS_H   

#include <stdio.h>


typedef enum {
    COLOR_RED ,       //1
    COLOR_GREEN ,     //2
    COLOR_BLUE ,      //3
    COLOR_YELLOW,      //4
    COLOR_NONE
}card_color ;

typedef enum {
    Number ,
    Skip ,
    Draw_2 ,
    Reverse ,
    Wild_draw_4 ,
    Wild 
}card_type ;

typedef struct{
    card_color color ;
    card_type type ;
    int number ;
}Card;

#endif