#include<iostream>
#include<stdio.h>
#include<string.h>
#include<assert.h>
#include<asio.hpp>
#include <cstdint>
#include <cmath> 
#include <unordered_map>


//Declarations
//--------------------------------------------------------------------------------------------------------------
asio::ip::tcp::iostream establish_connection();
void get_g_value(const double distance, const uint8_t type, uint8_t *g);
void parse_line(std::string line, int64_t *id,  int32_t *x, int32_t *y, uint8_t *type);
void timer_handler(asio::steady_timer &timer, const std::chrono::milliseconds &interval);
void server_read_thread(asio::ip::tcp::iostream &input, asio::io_context &io);

//Structs
//--------------------------------------------------------------------------------------------------------------
//For representing the server object
struct serverObject{
    int64_t id;
    int32_t x, y;
    uint8_t type;
    //RGB for representing colors
    uint8_t r = 0x5B;
    uint8_t g; 
    uint8_t b = 0x6D;
};

//A buffer for our shared data
struct DataBuffer{
    std::unordered_map<int64_t, serverObject> objects_map;
    std::mutex mtx;

    void add_object(serverObject object){
        //Try and wait for lock, destroy ownership when exiting scope
        std::lock_guard<std::mutex> lock(mtx);
        objects_map[object.id] = std::move(object);
    }
    std::vector<serverObject> extract_batch(){
        //Try and wait for lock, destroy ownership when exiting scope
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<serverObject> batch;
        batch.reserve(objects_map.size());
        
        for (auto& pair : objects_map) {
            batch.push_back(std::move(pair.second));
        }
        
        //Clear the map now that the data has been extracted
        objects_map.clear(); 
        return batch;
    }
};
//--------------------------------------------------------------------------------------------------------------
//Global buffer
DataBuffer global_data_buffer;
//--------------------------------------------------------------------------------------------------------------


int main(int argc, char const *argv[])
{   
    asio::io_context io;
    asio::steady_timer timer(io);
    asio::ip::tcp::iostream input = establish_connection();
    std::thread read_thread(server_read_thread, std::ref(input), std::ref(io));
    //Since we are outputting to the user every 1.7 seconds but reading continously we should do this asynchronosly
    
    
    const auto interval = std::chrono::milliseconds(1700);
    
    timer.expires_at(timer.expiry() + interval);
    //We handle this with a lambda
    timer.async_wait([&timer, interval](const asio::error_code&){
        timer_handler(timer, interval);
    });
    
    
    io.run();
    
    if(read_thread.joinable())
        read_thread.join();
    
    return 0;
}


//Implementations
//--------------------------------------------------------------------------------------------------------------
asio::ip::tcp::iostream establish_connection(){
    /*
    Function to establish tcp connection to server 
    */
    asio::ip::tcp::iostream input("localhost", "5463");
    if (!input){
        std::clog << "Error in establish_connection: No connection established" << std::endl;
        throw std::runtime_error("Connection establishment failed.");
    }
    return input;
}

void get_g_value(const double distance, const uint8_t type, uint8_t *g){
    /*
    Function which modifies g value in rgb (for us r, b is constant) based on distance and type
    */
    switch (type)
    {
    case (uint8_t)1:
        if(distance < 75){
            if(distance < 50)
                *g = 0x31; //red
            else
                *g = 0x33; //yellow
            }
        else
            *g = 0x34; //blue
        break;
    case (uint8_t)2:
        if(distance < 50)
            *g = 0x33; //yellow
        else
            *g = 0x34; //blue
        break;
    case (uint8_t)3:
        if (distance < 100)
            *g = 0x31; //red
        else
            *g = 0x33; //yellow
        break;
    default:
        break;
    }
}

void parse_line(std::string line, int64_t *id,  int32_t *x, int32_t *y, uint8_t *type){
    /*
    Function which parses string line into ID, X, Y and type. 
    */
   
    std::string delimiter = ";";
    unsigned int substring_index = 0;
    
    unsigned int LINE_ELEMENTS = 4;

    while (substring_index < LINE_ELEMENTS){
        //Get first substring, when we split by ;
        std::string substring = line.substr(0, line.find(delimiter));
        std::string numeric_value = substring;
        //Get the thing following the equal sign =
        numeric_value.erase(0, line.find("=")+1);
        //std::cout << numeric_value << std::endl;
        //Remove the parsed substring from original string
        line.erase(0, substring.length()+1);
        
        //Handle substring based on which index we are using
        switch (substring_index)
        {
        case 0:
            (*id) = (int64_t)(std::stol(numeric_value));
            break;
        
        case 1:
            (*x) = (int32_t)(std::stoi(numeric_value));
            break;
        case 2:
            (*y) = (int32_t)(std::stoi(numeric_value));
            break;
        case 3:
            (*type) = (uint8_t)(std::stoi(numeric_value));
            break;
        default:
            break;
        }
        substring_index++;
    }
   
        
}

void timer_handler(asio::steady_timer &timer, const std::chrono::milliseconds &interval){
    /*Handler function which: 
    1.Reads objects from global buffer
    2.Schedules a next read of objects at time = t0 + interval
    */
    std::vector<serverObject> batch = global_data_buffer.extract_batch();
    
    //Data specification
    const uint32_t preamble = 0xFE00;
    const uint32_t count = batch.size();
    
    if (count > 0) {
        fwrite(&preamble, sizeof(uint32_t), 1, stdout);     
        fwrite(&count, sizeof(uint32_t), 1, stdout);
        size_t elements_written = fwrite(
            batch.data(),          
            sizeof(serverObject),  
            count,                 
            stdout
        );
        fflush(stdout);
    }
    else{
        //No objects to write -- raise error to clog
        std::clog << "Error in timer_handler: No objects read from buffer" << std::endl;
    }
    //Schedule new expiry
    timer.expires_at(timer.expiry() + interval);
    //We handle this with a lambda
    timer.async_wait([&timer, interval](const asio::error_code&){
        timer_handler(timer, interval);
    });
}


void server_read_thread(asio::ip::tcp::iostream &input, asio::io_context &io){
    /*
    Thread for reading from server and writing to global buffer
    */   
    int64_t id;
    int32_t x, y;
    uint8_t type;

    double distance, x_dist, y_dist;
    serverObject object;
    //For holding the g value in rgb
    uint8_t g;
    for(std::string str; std::getline(input, str);){
        
        parse_line(str, &id, &x, &y, &type);
        x_dist = 150.0 - x;
        y_dist = 150.0 - y;
        distance = std::sqrt(x_dist*x_dist + y_dist*y_dist);
        get_g_value(distance, type, &g);

        //Fill object
        object.id = id;
        object.x  = x;
        object.y  = y;
        object.type = type;
        object.g = g;

        //Write object to buffer
        global_data_buffer.add_object(object);
        
    }
    //Nothing more to read
    if(input.fail() && !input.eof()){
        std::clog << "Error in server_read_thread: Input stream failed without reaching EOF" << std::endl;
    }
    else{
        std::clog << "Server has shutdown. " << std::endl;
        io.stop();
    }
    
}