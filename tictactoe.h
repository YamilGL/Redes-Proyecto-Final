#include <iostream>
#include <vector>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>
#include <map>
#include <random>
#include <algorithm>
#include <numeric>
#include <random>
using namespace std;

class Neuron;

typedef vector<Neuron> Layer;

struct Conection{
  double weight;
  double deltaWeight;
};

class Neuron{
private:
  double output;
  unsigned index;
  double gradient;
  double error;
  vector<Conection> outputWeights;
  static double n; // taza de aprendizaje [0.0 : 1.0]
  static double alpha; // multiplicador del ultimo cambio de peso [0.0 : n]

  static double randomWeight(void) {return rand() / double(RAND_MAX);}
  // static double randomWeight(void) {return 2.0 * (rand() / double(RAND_MAX)) - 1.0;}
  static double activationFunction(double x) {return 1 / (1 + exp(-x));} //sigmoid
  //static double activationFunction(double x) {return 1 / (1 + exp(-x));} //relu
  static double activationFunctionDerivative(double x){return x * (1 - x);}
  double sumDOW(const Layer &nextLayer){
    double sum=0.0;
    // sumar las contribuciones de los errores de los nodos que alimentamos
    for(unsigned neuron=0; neuron <nextLayer.size()-1; ++neuron){
      sum+=outputWeights[neuron].weight * nextLayer[neuron].gradient;
    }
    return sum;
  }

public:

  Neuron(unsigned nOutput, unsigned index){
    for(unsigned conection = 0; conection < nOutput; ++conection){
      outputWeights.push_back(Conection());
      outputWeights.back().weight = randomWeight();
    }
    this->index=index;
  }

  void setOutputVal (double val) {output=val;}
  double getOutputVal(void) const {return output;}
  
  void feedForward(const Layer &prevLayer){ //sumatoria de las capas anteriores
    double sum=0.0;
    for(unsigned neuron=0; neuron<prevLayer.size(); ++neuron){
      sum += prevLayer[neuron].getOutputVal() * prevLayer[neuron].outputWeights[index].weight;
    }
    output = activationFunction(sum);
  }

  void calcOutputGradients(double target){
    double delta = target - output;
    gradient = delta * activationFunctionDerivative(output);
  }

  void calcHiddenGradients(const Layer &nextLayer){
    double dow = sumDOW(nextLayer); //suma de las derivadas de los pesos
    gradient = dow * activationFunctionDerivative(output);
  }

  void updateInputWeights(Layer &prevLayer){
    // actualizar los pesos que estan en la estructura Conection de las neuronas de las capas anteriores
    for(unsigned n=0; n < prevLayer.size(); ++n){
      Neuron &neuron = prevLayer[n];
      double oldDeltaWeight = neuron.outputWeights[index].deltaWeight;
      double newDeltaWeight = n * neuron.getOutputVal() * gradient + alpha * oldDeltaWeight;
      neuron.outputWeights[index].deltaWeight = newDeltaWeight;
      neuron.outputWeights[index].weight += newDeltaWeight;
    }
  }

  void calcError(double target){
    error = target - output;
  }

  void updateInputWeights2(Layer &prevLayer){
    // actualizar los pesos que estan en la estructura Conection de las neuronas de la capa inicial
    for(unsigned n=0; n < prevLayer.size(); ++n){
      Neuron &neuron = prevLayer[n];
      double newDeltaWeight = n * neuron.getOutputVal() * error;
      neuron.outputWeights[index].weight += newDeltaWeight;
    }
  }

  void saveInputWeights(Layer &prevLayer, ofstream& file){
    for(unsigned n=0; n < prevLayer.size(); ++n){
      Neuron &neuron = prevLayer[n];
      file<<neuron.outputWeights[index].weight<<endl;
    }
  }

  void readInputWeights(Layer &prevLayer, ifstream& file){
    for(unsigned n=0; n < prevLayer.size(); ++n){
      Neuron &neuron = prevLayer[n];
      file>>neuron.outputWeights[index].weight;
    }
  }
};

class Perceptron{
private:
  vector<Layer> layers; // Layers[layer][neuron]
  double recentAverageSmoothingFactor=100.0;

public:
  double error;
  double recentAverageError;

  Perceptron(){}
  
  Perceptron(const vector<unsigned> &topology){
    unsigned nLayers = topology.size();
    for(unsigned layer = 0; layer < nLayers; ++layer){
      layers.push_back(Layer()); //agregar layers
      unsigned nOutputs = layer == topology.size() - 1 ? 0 : topology[layer + 1]; // #outputs que tendra cada neuron
      for(unsigned neuron = 0; neuron <= topology[layer]; ++neuron){
        layers.back().push_back(Neuron(nOutputs,neuron)); //agregar neurons
      }
      layers.back().back().setOutputVal(1.0);
    }
  }

  void feedForward(const vector<double> &input){
    assert(input.size() == layers[0].size()-1);
    
    // ingresar nuevos valores a la capa inicial
    for(unsigned neuron=0; neuron<input.size(); ++neuron){
      layers[0][neuron].setOutputVal(input[neuron]);
      // cout<<layers[0][neuron].getOutputVal()<<" ";
    }

    // ingresar nuevos valores a las siguientes capas
    for(unsigned layer=1; layer < layers.size(); ++layer){
      Layer &prevLayer = layers[layer-1];
      // cout<<"Valores capa "<<layer<<": ";
      for(unsigned neuron=0; neuron < layers[layer].size() - 1; ++neuron){
        layers[layer][neuron].Neuron::feedForward(prevLayer);
        // cout<<layers[layer][neuron].getOutputVal()<<" ";
      }
      // cout<<endl;
    }
  }

  void backProp(const vector<double> &target){
    // Calcular error promedio de la red
    Layer &outputLayer = layers.back();
    error = 0.0;
    for(unsigned neuron=0; neuron < outputLayer.size() - 1; ++neuron){
      double delta = target[neuron] - outputLayer[neuron].getOutputVal(); //delta = yTarget - yOutput
      error += delta * delta;
    }
    error /= outputLayer.size() - 1; // raiz error promedio
    error = sqrt(error); // RMS

    // calcular la diferencia de errores entre la ultima generacion y la anterior
    recentAverageError = (recentAverageError * recentAverageSmoothingFactor + error) 
                       / (recentAverageSmoothingFactor + 1.0);

    // calcular gradientes de la capa final
    for(unsigned neuron=0; neuron < outputLayer.size() - 1; ++neuron){
      outputLayer[neuron].calcOutputGradients(target[neuron]);
    }

    // calcular gradientes de las capas ocultas
    for(unsigned layer=layers.size()-2; layer>0; --layer){
      Layer &hiddenLayer = layers[layer];
      Layer &nextLayer = layers[layer+1];
      for(unsigned neuron=0; neuron < hiddenLayer.size(); ++neuron){
        hiddenLayer[neuron].calcHiddenGradients(nextLayer);
      }
    }

    // actualizar los pesos de las conexiones de adelante para atras
    for(unsigned layer=layers.size()-1; layer > 0; --layer){
      Layer &currentLayer = layers[layer];
      Layer &prevLayer = layers[layer-1];
      for(unsigned neuron=0; neuron < currentLayer.size(); ++neuron){
        currentLayer[neuron].updateInputWeights(prevLayer);
      }
    }
  }

  // Actualizar pesos en un perceptron simple 
  void simpleUpdateWeights(const vector<double> &target){ 
    Layer &inputLayer = layers[0];
    Layer &outputLayer = layers[1];

    // Calcular error promedio de la red
    error = 0.0;
    for(unsigned neuron=0; neuron < outputLayer.size() - 1; ++neuron){
      double delta = target[neuron] - outputLayer[neuron].getOutputVal(); //delta = yTarget - yOutput
      error += delta * delta;
    }
    error /= outputLayer.size() - 1; // raiz error promedio
    error = sqrt(error); // RMS

    // calcular la diferencia de errores entre la ultima generacion y la anterior
    recentAverageError = (recentAverageError * recentAverageSmoothingFactor + error) 
                       / (recentAverageSmoothingFactor + 1.0);

    // calcular error
    for(unsigned neuron=0; neuron < outputLayer.size() - 1; ++neuron){
      outputLayer[neuron].calcError(target[neuron]);
    }
    // actualizar pesos
    for(unsigned neuron=0; neuron < outputLayer.size(); ++neuron){
      outputLayer[neuron].updateInputWeights2(inputLayer);
    }
  }

  void saveWeights(){
    ofstream file("weights.txt");
    for(unsigned layer=1; layer<layers.size(); ++layer){
      Layer &currentLayer = layers[layer];
      Layer &prevLayer = layers[layer-1];
      for(unsigned neuron=0; neuron<currentLayer.size()-1; ++neuron){
        currentLayer[neuron].saveInputWeights(prevLayer, file);
      }
    }
    file.close();
  }

  void readWeights(string fileName){
    ifstream file(fileName);
    for(unsigned layer=1; layer<layers.size(); ++layer){
      Layer &currentLayer = layers[layer];
      Layer &prevLayer = layers[layer-1];
      for(unsigned neuron=0; neuron<currentLayer.size()-1; ++neuron){
        currentLayer[neuron].readInputWeights(prevLayer, file);
      }
    }
  }
  
  void getResults(vector<double> &result) const{
    result.clear();
    for(unsigned i=0; i < layers.back().size()-1; ++i){
      result.push_back(layers.back()[i].getOutputVal());
    }
  }

  vector<double> softmax(const vector<double>& input) {
    vector<double> exp_values(input.size());

    double max_input = *max_element(input.begin(), input.end());

    for (size_t i = 0; i < input.size(); ++i) {
      exp_values[i] = exp(input[i] - max_input);
    }

    double sum_exp = accumulate(exp_values.begin(), exp_values.end(), 0.0);

    for (size_t i = 0; i < input.size(); ++i) {
      exp_values[i] /= sum_exp;
    }
    
    return exp_values;
  }

  bool checkError(const vector<double>& result, const vector<double>& target){
    for(int i=0; i<target.size(); ++i){
      if(result[i]!=target[i]) {
        return true;
      }
    }
    return false;
  }

  bool checkError2(const vector<double>& result, const vector<double>& target){
    int max=0;
    double maxVal=0.0;
    for(int i=1; i<target.size(); ++i){
      if(result[i]>maxVal) {
        maxVal=result[i];
        max=i;
      }
    }
    // cout<<"Prediccion: "<<max<<endl;
    if(target[max]!=1.0){
      return true;
    }
    return false;
  }

  void trainPerceptron(const vector<vector<double>>& boards,
                      const vector<int>& nextMoves,
                      map<double,vector<double>>& target,
                      vector<double>& result){
    
    int generacion=0;
    while(generacion<100){
      // calcular resultado
      cout<<"Generacion "<<++generacion<<endl;
      for(unsigned i=0; i<boards.size(); ++i){
      vector<double> result;
      // cout<<"Numero: "<<nextMoves[i]<<endl;
      feedForward(boards[i]);
      getResults(result);

      // actualizar pesos
      vector<double> sftmx = softmax(result);
      if(checkError2(sftmx,target[nextMoves[i]])){
        backProp(target[nextMoves[i]]);
      }
      // showVector(" Resultado: ", sftmx);
      // showVector("  Esperado: ", target[nextMoves[i]]);
      // cout<<"    Error : "<<simple.error<<endl;
      // cin.ignore();
      }
    }
    saveWeights();
    cout<<"Trained!"<<endl;
  }
};

double Neuron::n = 0.02;
double Neuron::alpha = 0.0;

vector<double> stringToVector(const string& row){
  vector<double> board;
  for(char c:row){
    if(c=='0'){
      board.push_back(0.0);
    }
    else if(c=='1'){
      board.push_back(1.0);
    }
  }
  return board;
}

void showVector(string label, vector<double> &v){
  cout << label << " ";
  for (int i = 0; i < v.size(); ++i) {
    cout << v[i] << " ";
  }
  cout << endl;
}

vector<int> randomSelect(const vector<int>& labels){
  vector<int> selected;
  random_device rd;
  mt19937 gen(rd());

  for(int n=0; n<9; ++n){
    vector<int> index;
    for(int label=0; label<labels.size(); ++label){
      if(labels[label]==n)
        index.push_back(label);
    }
    shuffle(index.begin(),index.end(),gen);
    selected.insert(selected.end(),index.begin(),index.begin()+100);
  }
  shuffle(selected.begin(),selected.end(),gen);

  return selected;
}

void processDataAprendisaje(
  const std::multimap<int, std::string>& DataAprendisaje,
  std::vector<std::vector<double>>& boards,
  std::vector<int>& nextMoves,
  std::map<double,vector<double>>& target) {
  std::vector<std::pair<int, std::string>> tempData;
  for (const auto& entry : DataAprendisaje) {
    tempData.push_back(entry);
  }

  // Barajar aleatoriamente
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(tempData.begin(), tempData.end(), g);

  for (const auto& entry : tempData) {
    int key = entry.first;
    std::string value = entry.second;

    std::vector<double> board;
    for (char c : value) {
      board.push_back(static_cast<double>(c - '0'));
    }
    boards.push_back(board);
    nextMoves.push_back(key);
  }
  
  for (int key = 0; key < 9; ++key) {
    vector<double> vec(10, 0.0);
    vec[key] = 1.0;
    target[key] = vec;
  }
}

Perceptron buildPerceptron(){
  vector<unsigned>topology;
  topology.push_back(27);
  topology.push_back(11);
  topology.push_back(9);
  Perceptron MLP(topology);
  return MLP;
}