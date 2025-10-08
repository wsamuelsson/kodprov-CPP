# Solution to kodprov

## Compile
- Run make 

## Run 
- Start server by running: `java -classpath SAAB.jar com.saabtech.server.SAABServer`
- Start middleware and plotting script by doing `./middleware.out | python3 plot_coordinates.py`

## Comments on code

- `middleware.cpp` is most relevant and is developed by me
- `plot_coordinates.py` contains generated boilerplate code for reading the binary data from stdout and plotting coordinates, mainly to see that things behave as expected. 