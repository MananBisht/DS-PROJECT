#include "player_hand.h"

void init_hand(hand *hand){
    hand -> head = NULL; 
    hand -> tail = NULL;
    hand -> size = 0 ;
}
void add_card(hand *hand,Card card){
//    Card draw = draw_card(deck);
    hand_node *node =(hand_node *)malloc(sizeof(hand_node));
    node ->card = card ;
    node->next = node->prev = NULL;
    
    if(hand->size == 0){
        hand ->head = hand->tail = node ;
        hand -> size ++;
        return ;
    }

    node -> prev = hand -> tail ;
    hand -> tail -> next = node ;
    hand -> tail = node ;

    hand -> size++ ;
}
void add_discard_card(hand *hand, Card card){
    hand_node *node =(hand_node *)malloc(sizeof(hand_node));
    node ->card = card ;
    node->next = node->prev = NULL;
    
    if(hand->size == 0){
        hand ->head = hand->tail = node ;
        hand -> size ++;
        return ;
    }

    node -> prev = hand -> tail ;
    hand -> tail -> next = node ;
    hand -> tail = node ;

    hand -> size++ ;
}
int compare_cards(Card a, Card b) {
    if (a.color != b.color) return 0;
    if (a.type != b.type) return 0;
    if (a.type == Number && a.number != b.number) return 0;
    return 1;
}
int can_play(Card card , Card discard_pile_top){
    if(card.type == Wild_draw_4 || card.type == Wild) return 1 ;
    if(card.color==discard_pile_top.color) return 1 ;
    if(card.type != Number && card.type == discard_pile_top.type) return 1 ;
    if (card.type == Number && card.number == discard_pile_top.number) return 1;
    return 0;    
}
int get_valid_cards(hand *hand, Card discard_pile_top, Card playable[] ){
    hand_node *itr = hand->head ;
    int count = 0 ;
    while(itr){
        if(can_play(itr->card,discard_pile_top)){
            playable[count] = itr->card ;
            count++;
        }
        itr = itr -> next ;
    }
    return count ;
}
void remove_card(hand *hand, Card card){
    hand_node *itr = hand->head ;

    if(hand->size==1){
        hand->head = hand ->tail = NULL ;
        free(itr) ;
        hand->size -- ;
        return ;
    }
    if(compare_cards(card , hand->head->card)){  
        hand->head = itr->next ;
        free(itr);
        hand->size-- ;
        hand->head->prev = NULL;    
        return ; 
    }
    if(compare_cards(card , hand->tail->card)){
        hand_node *temp = hand->tail ;
        hand->tail = hand->tail->prev ;
        hand->tail->next = NULL;
        free(temp);
        hand->size--;
        return ;
    }
    while(compare_cards(card , itr->next->card)==0){
        itr = itr ->next ;
    }
    hand_node *temp = itr->next ;
    itr->next = itr ->next->next ;
    itr ->next->prev = itr ;
    free(temp);
    hand->size-- ;
}
const char* color_to_string(card_color color) {
    switch (color) {
        case COLOR_RED: return "Red";
        case COLOR_GREEN: return "Green";
        case COLOR_BLUE: return "Blue";
        case COLOR_YELLOW: return "Yellow";
        default: return "Wild";
    }
}

const char* type_to_string(card_type type) {
    switch (type) {
        case Number: return "Number";
        case Skip: return "Skip";
        case Wild_draw_4: return "Wild Draw 4";
        case Draw_2: return "Draw 2";
        case Reverse: return "Reverse";
        case Wild: return "Wild";
        default: return "UnknownType";
    }
}

void display_hand(hand *hand){
    hand_node * temp = hand -> head;
    while(temp){
        if(temp->card.type == Wild || temp -> card.type == Wild_draw_4){
            printf("%s ",type_to_string(temp->card.type));
        }else {
            printf("%s ",color_to_string(temp->card.color)); 
            printf("%s ",type_to_string(temp->card.type));   
            if(temp->card.type == Number){
                printf(" %d ",temp->card.number);
            }
        }
        temp = temp->next ;
        printf("\n");
    } 
}
Card get_card_by_index(hand *hand, int idx){
    hand_node *itr = hand->head;
    for(int i=0;i<idx && itr;i++){
        itr = itr->next;
    }
    if(itr) return itr->card;
    Card empty; empty.type = Number; empty.number = -1; return empty;
}
