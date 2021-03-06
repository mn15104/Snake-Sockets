#include "HostRoom.h"

HostRoom::HostRoom(sf::RenderWindow* t_window, Client& t_client) 
    : window(t_window), client(t_client), spriteMaker(SpriteSheet("./button.png")), grid(t_window) {

    font.loadFromFile("./Font/Roboto-Thin.ttf");
    sf::Text title("CHESS ENGINE", font);
    title.setCharacterSize(30);
    title.setPosition(200, 0);
    title.setStyle(sf::Text::Bold);
    title.setColor(sf::Color::Red);
    
}

void HostRoom::processInput(sf::Vector2i position){
    if((client.clientState == ClientState::PlayingAsHost && grid.playerState == PlayerState::Blue) || 
       (client.clientState == ClientState::PlayingAsOpponent && grid.playerState == PlayerState::Red)){
        grid.processInput(position);
        client.pushGameMove(position);
    }
}

void HostRoom::update(){
    //Check if any new rooms
    if(client.clientState == ClientState::Connected){
        std::string lobbyRoom = client.lobbyRoomUpdate();
        if(lobbyRoom != ""){
            std::cout << "NEW ROOM" << lobbyRoom << std::flush;
            sf::Text room(lobbyRoom, font);
            room.setCharacterSize(30);
            room.setPosition(200, widgets.size()*50);
            room.setStyle(sf::Text::Bold);
            room.setColor(sf::Color::Red);
            widgets.push_back(room);
        }
    }
    else if(client.clientState == ClientState::PlayingAsHost ||
            client.clientState == ClientState::PlayingAsOpponent){
        grid.drawGrid();
        //Get opponent moves
        if((client.clientState == ClientState::PlayingAsHost && 
            grid.playerState == PlayerState::Red) || 
           (client.clientState == ClientState::PlayingAsOpponent && 
            grid.playerState == PlayerState::Blue)){
                
            sf::Vector2i opponentMove = client.gameMoveUpdate();
            if(opponentMove.x != -1){
                std::cout << opponentMove.x << "," << opponentMove.y << std::flush;
                grid.processInput(opponentMove);
            }
        }
    }
}

void HostRoom::draw(){
    window->draw(title);
    window->draw(hostRoomButton);
    for(auto it = widgets.begin(); it != widgets.end(); it++){
        window->draw(*it); 
    }
}

