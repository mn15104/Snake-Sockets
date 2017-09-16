#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "Snake.h"
#include <SDL2/SDL.h>
#include <SFML/Graphics.hpp>
#include <cstdlib>
#include "Client.h"
#include <functional>
#include <thread>
// #undef main
 
void createClient(Client& client){
    client.run();
    std::cout << "running " << std::flush;
}

void doDraw(char** map, sf::Window* window){
    for(int i = 0; i < 20; i ++){
        for(int j = 0; j < 20; j++){
            printf("%d %d %d \n", *(*(map+i)+j), i, j);
            std::cout << std::flush;
        }
    }
}


//Seg fault caused by wrong order/synchronisation of RenderWindow actions?
void processMap(Client& client, Snake& c2, char** map){
    while(true){
        if(client.isConnected()){
            if(!client.mapQueueIsEmpty()){
                map = client.getMap();
                for(int i = 0; i < 20; i ++){
                    for(int j = 0; j < 20; j++){
                        printf("%d", *(*(map + i)+j));
                        std::cout << std::flush;
                        // c2.blitCell(i*40, j*40);
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {

    sf::RenderWindow window(sf::VideoMode(800, 800), "SFML works!");
    sf::RenderTexture renderTexture;
    sf::Clock clock;
    sf::Time elapsedTime = clock.restart();

    char** map = new char*[20];
    for(int i = 0; i < 20; ++i)
        map[i] = new char[20];

    Client client; 
    sf::Thread runClient(&createClient, std::ref(client));
    runClient.launch(); 

    Snake snake(&window);
    Snake snake2(&window);

    sf::Thread runMap(std::bind(&processMap, std::ref(client), std::ref(snake2), map));
    runMap.launch();

    while (window.isOpen())
    {   
        if(client.isConnected()){
            clock.restart();
            sf::Event event;
            while (window.pollEvent(event)){
                //Process our character actions
                if(event.type == sf::Event::KeyPressed){
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)){
                        snake.updateState(State::L);
                        client.outBuffer('L');
                    }
                }
                if(event.type == sf::Event::Closed)
                    window.close();
            }
            // //Update and draw both characters
            // sf::Time now = clock.getElapsedTime();
            
            window.display();
            window.clear();
            
        }
    }

    return 0;
}