# Definimos el compilador a utilizar
CXX = g++

# Definimos las opciones del compilador
CXXFLAGS = -Wall -pthread -std=c++17

# Definimos los nombres de los archivos fuente y sus respectivos ejecutables
SRCS = terminal.cpp ServidorRedNeuronal.cpp serv.cpp
EXECS = Terminal RedNeuro Serv

# Reglas para compilar todos los programas
all: $(EXECS)

# Regla para generar el ejecutable de terminal.cpp
Terminal: terminal.o
	$(CXX) $(CXXFLAGS) -o Terminal terminal.o

# Regla para generar el ejecutable de ServidorRedNeuronal.cpp
RedNeuro: ServidorRedNeuronal.o
	$(CXX) $(CXXFLAGS) -o RedNeuro ServidorRedNeuronal.o

# Regla para generar el ejecutable de serv.cpp
Serv: serv.o
	$(CXX) $(CXXFLAGS) -o Serv serv.o

# Regla para compilar los archivos fuente en archivos objeto
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regla para limpiar los archivos generados
clean:
	rm -f $(OBJS) $(EXECS)

# Reglas para limpiar los archivos objeto
clean_objs:
	rm -f $(OBJS)