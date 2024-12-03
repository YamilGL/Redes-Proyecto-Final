#include <iostream>
#include <vector>
#include <random>
#include <map>

using namespace std;

struct TresEnRaya {
  vector<string> tablero;
  vector<string> jugador;
  vector<string> pass;
  vector<char> ficha;
  vector<int> id;
  vector<int> socket;

  void movimiento(string &tabla, int posicion, char ficha);
};

void TresEnRaya::movimiento(string &tabla, int posicion, char ficha) {
    tabla[posicion] = ficha;
}