#include "player.h"
void init_player(Player *p , char *p_name, int id){
    p->id = id ;
    strcpy(p->name , p_name);
    init_hand(&(p->hand));
}

void init_card_draw(Player * p , Deck *deck){
    for(int i = 0 ; i < 7 ; i ++){
        Card c = draw_card(deck);
        add_card(&(p->hand) , c);
    }
}

void init_card_draw_reload(Player * p , Deck *deck, int num_players){
    for(int i = 0 ; i < 50/num_players && deck->size > 0; i++ ){
        Card c = draw_card(deck);
        add_card(&(p->hand) , c);
    }
}
