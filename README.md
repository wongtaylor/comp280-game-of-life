# comp280-game-of-life

Program implements a parallel version of Conway’s Game of Life using threads and barriers to go through each spot on the virtual board. It will check the state of the neighbors surrounding the specified cell and based on the number of live and dead cells, the cell will be updated accordingly.

Conway’s Game of Life:
A 2D world implemented as a torus and every cell in the grid has exactly eight neighbors. Conway’s Game of Life is an example of discrete event simulation, where a world of entities live, die, or are born based on their surrounding neighbors. Each time step simulates another round of living or dying.
