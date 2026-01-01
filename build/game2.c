#include "game2.h"
#include <stdbool.h> // for bool

void init_game(Game* game, int num_players, char player_names[][32]) {
    init_deck(&game->draw_pile);
    shuffle_deck(&game->draw_pile);

    game->discard_pile.size = 0;
    game->discard_pile.top = 0; 

    game->num_players = num_players;
    game->current_turn = 0; 
    game->direction = 1;

    for (int i = 0; i < num_players; i++) {
        init_player(&game->players[i], player_names[i], i);
        init_card_draw(&game->players[i], &game->draw_pile);
    }

    while (game->draw_pile.cards[game->draw_pile.top].type == Wild ||
           game->draw_pile.cards[game->draw_pile.top].type == Wild_draw_4 ||
           game->draw_pile.cards[game->draw_pile.top].type == Skip ||
           game->draw_pile.cards[game->draw_pile.top].type == Draw_2 ||
           game->draw_pile.cards[game->draw_pile.top].type == Reverse) {
        shuffle_deck(&game->draw_pile);
    }
    game->discard_pile.cards[game->discard_pile.top] = draw_card(&game->draw_pile);
    game->discard_pile.size = 1;
}

void next_turn(Game* game) {
    game->current_turn = (game->current_turn + game->direction + game->num_players) % game->num_players;
}

Player* get_current_player(Game* game) {
    return &game->players[game->current_turn];
}

int apply_card_effect(Game* game, Card card) {
    int advanced = 0;
    Player* p;

    // Ensure draw pile has cards
    if (game->draw_pile.size < 4) {
        reshuffle_deck(game);
    }

    switch (card.type) {
    case Skip:
        next_turn(game);
        advanced++;
        break;

    case Draw_2:
        next_turn(game);
        advanced++;
        p = get_current_player(game);
        add_card(&p->hand, draw_card(&game->draw_pile));
        add_card(&p->hand, draw_card(&game->draw_pile));
        break;

    case Reverse:
        game->direction *= -1;
        if (game->num_players == 2) {
            next_turn(game);
            advanced++;
        }
        break;

    case Wild_draw_4:
        next_turn(game);
        p = get_current_player(game);
        for (int i = 0; i < 4; i++) {
            if (game->draw_pile.size == 0) reshuffle_deck(game);
            add_card(&p->hand, draw_card(&game->draw_pile));
        }
        next_turn(game);
        advanced = 1; 
    break;

    
    case Wild:
        break;
    case Number:
  
        break;
    }
    return advanced;
}

void player_draw(Game* game, int player_id) {
    if (game->draw_pile.size == 0) {
        reshuffle_deck(game);
    }
    if (game->draw_pile.size > 0) {
        Player* p = &game->players[player_id];
        add_card(&p->hand, draw_card(&game->draw_pile));
    }
}

void reshuffle_deck(Game* game) {
    Card top = game->discard_pile.cards[game->discard_pile.top];

    game->draw_pile.size = game->discard_pile.size - 1;
    for (int i = 0; i < game->draw_pile.size; i++) {
        game->draw_pile.cards[i] = game->discard_pile.cards[i];
    }
    game->draw_pile.top = game->draw_pile.size - 1;
    
    shuffle_deck(&game->draw_pile);


    game->discard_pile.size = 1;
    game->discard_pile.top = 0;
    game->discard_pile.cards[0] = top;
}

int has_valid_card(hand* h, Card top) {
    hand_node* itr = h->head;
    while (itr) {
        if (can_play(itr->card, top)) return 1;
        itr = itr->next;
    }
    return 0;
}

int check_winner(Game* game, int player_id) {
    return game->players[player_id].hand.size == 0;
}

int play_card(Game* game, int player_id, Card card, card_color chosen_color) {
    Player* p = &game->players[player_id];
    Card top = game->discard_pile.cards[game->discard_pile.top];

    if (!can_play(card, top)) return 0; 

    remove_card(&p->hand, card);

    game->discard_pile.top++;
    game->discard_pile.cards[game->discard_pile.top] = card;
    game->discard_pile.size++;

    if (card.type == Wild || card.type == Wild_draw_4) {
        game->discard_pile.cards[game->discard_pile.top].color = chosen_color;
    }

    apply_card_effect(game, card);

    if (game->draw_pile.size <= 4) { 
        reshuffle_deck(game);
    }

    if (check_winner(game, player_id)) return 2;

    next_turn(game);

    return 1;
}
