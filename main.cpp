#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <fstream>  
#include <vector>
#include <cctype>
#include <cmath>
#include <chrono>
#include <sstream>
#include <climits>
#include <iomanip>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;
using namespace std::chrono;
//el lenguaje de las anotaciones es, completamente, informal, disculpas...

//asumimos que:
//no van a haber heroes y monstruos en el mismo "cuadro".


int columnas = 0; //en algunas funciones se usa lim_x y lim_y, antes no eran variables globales, en resumen.
int filas = 0;

int contadorheroes = 0; //me da paja hacer q el id sea con el vector asi que esto es el id.
int contadormonstruos = 0; //same XD

int heroes_vivos = 0;
int tick=0;

int randomizador = 0;

double tiempo = 0.5;
inline duration<double> tfinal = duration<double>(0.5);

bool color_consola = true;
bool borrar = false;

mutex mtx_combate; //mutex para proteger recursos compartidos
mutex mtx_grid;


class Monstruo; //declaracion adelantada

//se usa para la deteccion de los monstruos, así se tiene posicion e id del heroe
struct Objetivo {
    pair<int,int> posicion;
    int idHeroe;
};

class Heroe{
    private:
        int heroeVida, heroeDanioAtaque, heroeRangoAtaque;
        int heroePosX, heroePosY;
        vector<pair<int,int>> heroePath;
        int id;
        int paso;
        int turnos_sin_moverse;
        bool combatiendo = false;

    public:
        mutex mtx_log; //para que los logs no se traspapelen, teniendo en cuenta que se pueden hacer de varios hilos distintos
        ofstream logFile; // archivo de log propio
        string logFileName; // nombre del archivo

        Heroe(int heroeVida,
              int heroeDanioAtaque,
              int heroeRangoAtaque,
              int heroePosX,
              int heroePosY,
              const vector<pair<int,int>>& heroePath)
            : heroeVida(heroeVida),
              heroeDanioAtaque(heroeDanioAtaque),
              heroeRangoAtaque(heroeRangoAtaque),
              heroePosX(heroePosX),
              heroePosY(heroePosY),
              heroePath(heroePath),
              paso(0),
              turnos_sin_moverse(0)
        {
            id = contadorheroes;
            contadorheroes++;
            // Configurar archivo de log por heroe
            ostringstream fname;
            string logDir = "log_heroes";

            // Crear carpeta si no existe
            if (!fs::exists(logDir)) {
                fs::create_directory(logDir);
            }

            fname << logDir << "/heroe_" << (id + 1) << ".log";
            logFileName = fname.str();
            logFile.open(logFileName, ios::out | ios::trunc);

            if (!logFile) {
                // Si falla abrir, al menos avisa en consola una vez
                lock_guard<mutex> lk(mtx_log);
                cerr << " No se pudo abrir log para heroe " << (id + 1)
                    << " (" << logFileName << ")\n\n";
            } else {
                lock_guard<mutex> lk(mtx_log);
                cout << " Log creado para heroe " << (id + 1)
                    << ": " << logFileName << endl;
            }
        }

        ~Heroe(){
            if (logFile.is_open()) logFile.close();
        }
        string timestamp() {
            using namespace chrono;
            auto now = system_clock::now();
            time_t t = system_clock::to_time_t(now);
            tm tm{};

            //asumiendo SO linux.
            localtime_r(&t, &tm);
            
            ostringstream os;
            os << put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return os.str();
        }

        void logLine(const string& msg, bool also_console = false) {
            // Formato: [fecha] [M#id] (loop=N) mensaje
            ostringstream line;
            lock_guard<mutex> lk(mtx_log);
            line << "[" << timestamp() << "] "
                << "[H#" << (id+1) << "] "
                << msg << "\n";

            if (logFile.is_open()) {
                logFile << line.str();
                logFile.flush(); // para ver los logs en tiempo real
            }

            if (also_console) {
                
                cout << line.str();
                cout.flush();
            }
        }


        int getVida(){ return heroeVida; }
        int getDanioAtaque(){ return heroeDanioAtaque; }
        int getRangoAtaque(){ return heroeRangoAtaque; }
        int getPosX(){ return heroePosX; }
        int getPosY(){ return heroePosY; }
        int getID(){ return id; }
        vector<pair<int,int>> getPath(){ return heroePath; }
        int getPasos(){ return paso; }
        bool getCombatiendo(){ return combatiendo;}


        void mostrarPath() {
            ostringstream os;
            os << "Path del héroe: ";
            for (const auto& p : heroePath)
                os << "(" << p.first << "," << p.second << ")";
            os << "\n";
            logLine(os.str(), false);
        }

        void mostrarEstado() {
            ostringstream os;
            os << "ESTADO DEL HEROE " << id+1 << "" << endl;
            os << "                                      Vida: " << heroeVida << endl;
            os << "                                      Danio de Ataque: " << heroeDanioAtaque << endl;
            os << "                                      Rango de Ataque: " << heroeRangoAtaque << endl;
            os << "                                      Posicion: (" << heroePosX << ", " << heroePosY << ")" << endl;
            os << "                                      Path del heroe: ";
            for (const auto& p : heroePath)
                os << "(" << p.first << "," << p.second << ")";
            os << "\n\n";

            logLine(os.str(), false);
        }


        bool terminado() const {
            return (heroeVida <= 0) || (paso >= (int)heroePath.size() - 1);
        }

        void daniar(int reduccion){
            heroeVida -= reduccion;
            if(heroeVida <= 0){
                heroeVida = 0;
                heroes_vivos--;
            }
        }

        vector<int> limites(int lim_x,int lim_y){
            int X_low;
            int X_high;
            int Y_low;
            int Y_high;

            if(heroePosX - heroeRangoAtaque <= 0){
                X_low = 0;
            }
            else{
                X_low = heroePosX - heroeRangoAtaque;
            }
            
            if(heroePosX + heroeRangoAtaque >= lim_x){
                X_high = lim_x;
            }
            else {
                X_high = heroePosX + heroeRangoAtaque;
            }
            
            if(heroePosY - heroeRangoAtaque <= 0){
                Y_low = 0;
            }
            else {
                Y_low = heroePosY - heroeRangoAtaque;
            }
            
            if(heroePosY + heroeRangoAtaque >= lim_y){
                Y_high = lim_y;
            }
            else{
                Y_high = heroePosY + heroeRangoAtaque;
            }
            
            return {X_low, X_high, Y_low, Y_high};
        }

        int detectar_enRango(const vector<Monstruo*>& monstruos);

        void avanzar(const vector<Heroe*>& heroes){
            if (paso + 1 >= (int)heroePath.size()) return;

            int sgtX = heroePath[paso + 1].first;
            int sgtY = heroePath[paso + 1].second;

            // Si hay otro héroe en el siguiente paso, esperar
            for (size_t i = 0; i < heroes.size(); i++) {
                if (heroes[i] != this && heroes[i]->getVida() > 0 &&
                    heroes[i]->getPosX() == sgtX && heroes[i]->getPosY() == sgtY &&
                    !heroes[i]->terminado()) {
                    
                    ostringstream os;
                    turnos_sin_moverse++;
                    if (turnos_sin_moverse == 3) {      
                        os << "El heroe " << id+1 << " lleva 3 turnos sin moverse. Podría estar bloqueado.\n";
                    }
                    else if (turnos_sin_moverse >= 5) {
                        os << "El heroe " << id+1 << " estaba atascado y decidió avanzar igual.\n";

                        paso = min(paso + 1, (int)heroePath.size() - 1);
                        heroePosX = sgtX;
                        heroePosY = sgtY;
                        turnos_sin_moverse = 0;
                        if (heroePath.size() - paso - 1 >0){
                            cout << "Le quedan " << (heroePath.size() - paso - 1)
                            << " pasos para llegar a su meta.\n";
                        }
                        else{
                            cout << "Llego a la meta.\n";
                        }
                    }
                    logLine(os.str(), false);
                    return;
                }
            }

            // Mueve si la casilla está libre
            lock_guard<mutex> lock(mtx_grid);

            ostringstream os;
            os << "El heroe " << id+1 << " se movió desde ("<< heroePosX << "," << heroePosY
                 << ") hacia (" << sgtX << "," << sgtY << ").\n";

            heroePosX = sgtX;
            heroePosY = sgtY;
            paso++;
            turnos_sin_moverse = 0;

            os << "Le quedan " << (heroePath.size() - paso - 1)
                 << " pasos para llegar a su meta.\n";
            logLine(os.str(), false);
        }

        void atacar(Monstruo* &m);

        void* loop_heroe(vector<Heroe*>& heroes, vector<Monstruo*>& monstruos);
};




class Monstruo{
    private:
        int monstruoVida, monstruoDanioAtaque, monstruoRangoAtaque, monstruoRangoVision;
        int monstruoPosX, monstruoPosY;
        int id;
        bool habilitado;
        queue<pair<int,int>> pasosSgtes;
        bool combatiendo=false;
        bool siguiendo=false;

        int danioHecho;
        int loopExec;

        

    public:
        mutex mtx_log;
        Monstruo(int monstruoVida, int monstruoDanioAtaque, int monstruoRangoAtaque, int monstruoPosX, int monstruoPosY, int monstruoRangoVision){
            this->monstruoVida = monstruoVida;
            this->monstruoDanioAtaque = monstruoDanioAtaque;
            this->monstruoRangoAtaque = monstruoRangoAtaque;
            this->monstruoRangoVision = monstruoRangoVision;
            this->monstruoPosX = monstruoPosX;
            this->monstruoPosY = monstruoPosY;
            id = contadormonstruos;
            contadormonstruos++;
            habilitado=false;
            danioHecho=0;
            loopExec=0;
            // Configurar archivo de log por monstruo
            ostringstream fname;
            string logDir = "log_monstruos";

            // Crear carpeta si no existe
            if (!fs::exists(logDir)) {
                fs::create_directory(logDir);
            }

            fname << logDir << "/monstruo_" << (id + 1) << ".log";
            logFileName = fname.str();
            logFile.open(logFileName, ios::out | ios::trunc);

            if (!logFile) {
                // Si falla abrir, al menos avisa en consola una vez
                lock_guard<mutex> lk(mtx_log);
                cerr << " No se pudo abrir log para monstruo " << (id + 1)
                    << " (" << logFileName << ")\n";
            } else {
                lock_guard<mutex> lk(mtx_log);
                cout << " Log creado para monstruo " << (id + 1)
                    << ": " << logFileName << endl;
            }
        }
        ~Monstruo(){
            if (logFile.is_open()) logFile.close();
        }

        string timestamp() {
            using namespace chrono;
            auto now = system_clock::now();
            time_t t = system_clock::to_time_t(now);
            tm tm{};
            localtime_r(&t, &tm);
            ostringstream os;
            os << put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return os.str();
        }

        void logLine(const string& msg, bool also_console = false) {
            // Formato: [fecha] [M#id] (loop=N) mensaje
            ostringstream line;
            lock_guard<mutex> lk(mtx_log);
            line << "[" << timestamp() << "] "
                << "[M#" << (id+1) << "] "
                << "(loop=" << loopExec << ") "
                << msg << "\n";

            if (logFile.is_open()) {
                logFile << line.str();
                logFile.flush(); // para ver los logs en tiempo real
            }

            if (also_console) {
                
                cout << line.str();
                cout.flush();
            }
        }

        int getVida(){
            return monstruoVida;
        }
        int getDanioAtaque(){
            return monstruoDanioAtaque;
        }
        int getRangoAtaque(){
            return monstruoRangoAtaque;
        }
        int getRangoVision(){
            return monstruoRangoVision;
        }
        int getPosX(){
            return monstruoPosX;
        }
        int getPosY(){
            return monstruoPosY;
        }
        int getID(){
            return id;
        }
        queue<pair<int,int>> getPasosSgtes(){
            return pasosSgtes;
        }
        bool getHabilitado(){
            return habilitado;
        }
        int getDanioHecho(){
            return danioHecho;
        }

        void Habilitado(){
            habilitado=true;
        }
        bool getCombatiendo(){ return combatiendo; }
        bool getSiguiendo(){
            return siguiendo;
        }
        mutex mtx_pasos;

        ofstream logFile;      // archivo de log propio
        string   logFileName;  // nombre del archivo

        void monstrarEstado(){
            ostringstream os;
            os << "ESTADO DEL MONSTRUO " << id+1 << "\n"
            << "                                      Vida: " << monstruoVida << "\n"
            << "                                      Danio de Ataque: " << monstruoDanioAtaque << "\n"
            << "                                      Rango de Ataque: " << monstruoRangoAtaque << "\n"
            << "                                      Rango de Vision: " << monstruoRangoVision << "\n"
            << "                                      Daño Hecho: " << danioHecho << "\n"
            << "                                      Loop Exec: " << loopExec << "\n"
            << "                                      Posicion: (" << monstruoPosX << ", " << monstruoPosY << ")\n";
            logLine(os.str(), false);
        }

        void daniar(int reduccion){
            monstruoVida=monstruoVida-reduccion;
        }

        pair<int,int> lastQ(){
            logLine("se revisa el ultimo en cola.",false);
            lock_guard<mutex> lock(mtx_pasos);
            pair<int,int> resultado = {-1,-1};
            queue<pair<int,int>> aux = pasosSgtes;
            if(aux.size()>0){
                logLine("efectivamente.",false);
                for(size_t i = 0; i < pasosSgtes.size()-1 ; i++){
                    aux.pop();
                }
                resultado = aux.front();
            }
            return resultado;
        }

        vector<int> limites(int lim_x,int lim_y){
            int Y_low,Y_high;
            int X_low,X_high;
            //este codigo no considera coordenadas negativas.
            vector<int> resultado;
            //para que no se salga de los limites de X
            if(monstruoPosX - monstruoRangoVision < 0){
                X_low = 0;
            }
            else{
                X_low = monstruoPosX - monstruoRangoVision;
            }
            resultado.push_back(X_low);

            if(monstruoPosX + monstruoRangoVision > lim_x){
                X_high = lim_x; //columnas
            }
            else{
                X_high = monstruoPosX + monstruoRangoVision;
            }
            resultado.push_back(X_high);


            //para que no se salga de los limites de Y 
            if(monstruoPosY - monstruoRangoVision < 0){
                Y_low = 0;
            }
            else{
                Y_low = monstruoPosY - monstruoRangoVision;
            }
            resultado.push_back(Y_low);

            if(monstruoPosY + monstruoRangoVision > lim_y){
                Y_high = lim_y; //filas
            }
            else{
                Y_high = monstruoPosY + monstruoRangoVision;
            }
            resultado.push_back(Y_high);

            return resultado;
        }
        
        bool genPasosHacia(pair<int,int> objetivo) {
            unique_lock<mutex> lock(mtx_pasos, try_to_lock);
            if (!lock.owns_lock()) {
                // otro hilo está generando o usando pasos => saltar
                return false;
            }

            while (!pasosSgtes.empty()) pasosSgtes.pop(); // limpia pasos previos

            int guiaX = monstruoPosX;
            int guiaY = monstruoPosY;
            int pasos = 0;

            ostringstream os;
            os << "El monstruo " << id+1 << " está generando un path desde \n"
            << "                      Posicion: (" << monstruoPosX << ", " << monstruoPosY << ")\n"
            << "                      Hacia: (" << objetivo.first << ", " << objetivo.second << ")\n"
            << "                      El path es el siguiente: ";
            
            // mientras esté fuera del rango de ataque real
            
            while (sqrt(pow(guiaX - objetivo.first, 2) + pow(guiaY - objetivo.second, 2)) > monstruoRangoAtaque) {
                if (guiaX < objetivo.first) guiaX++;
                else if (guiaX > objetivo.first) guiaX--;
                else if (guiaY < objetivo.second) guiaY++;
                else if (guiaY > objetivo.second) guiaY--;

                os << "(" << guiaX << ", " << guiaY << ")";
                pasosSgtes.push({guiaX, guiaY});
                pasos++;
            }

            os << ".\n";
            logLine(os.str(), false);
            return pasos > 0;
        }

        Objetivo detectarMasCercano(const vector<Heroe*>& heroes) {
            int lim_x = columnas;
            int lim_y = filas;

            vector<int> lim_xy = limites(lim_x, lim_y);
            int X_low  = lim_xy[0];
            int X_high = lim_xy[1];
            int Y_low  = lim_xy[2];
            int Y_high = lim_xy[3];

            ostringstream os;
            os << "\n[M#" << id+1 << "] Escaneando héroes en rango: "
            << "X[" << X_low << "," << X_high << "], "
            << "Y[" << Y_low << "," << Y_high << "] desde ("
            << monstruoPosX << "," << monstruoPosY << ")." << endl;

            queue<int> enRango;
            {
                lock_guard<mutex> lock(mtx_grid);
                for (size_t i = 0; i < heroes.size(); i++) {
                    Heroe* h = heroes[i];
                    if (!h) continue;

                    if (h->getVida() <= 0) {
                        continue;
                    }
                    if (h->terminado()) {
                        continue;
                    }
                    int hx = h->getPosX(), hy = h->getPosY();
                    if (hx < X_low || hx > X_high || hy < Y_low || hy > Y_high) {
                        continue;
                    }
                    // NO entran heroes no declarados, sin vida, que hayan finalizado su path o esten fuera de rango.

                    enRango.push(i);
                    os << "  [H#" << i+1 << "] detectado en (" << hx << "," << hy << ")." << endl;
                }
            }

            Objetivo resultado;
            resultado.posicion = {-1, -1};
            resultado.idHeroe = -1;

            if (enRango.empty()) {
                os << "     Ningún héroe detectado en el rango." << endl;
                logLine(os.str(), false);
                return resultado;
            }

            if (enRango.size() == 1) {
                int idx = enRango.front();
                resultado.posicion = {heroes[idx]->getPosX(), heroes[idx]->getPosY()};
                resultado.idHeroe = idx;
                os << "     Un solo héroe detectado: H#" << idx+1
                << " en (" << heroes[idx]->getPosX() << "," << heroes[idx]->getPosY() << ")." << endl;
                logLine(os.str(), false);
                return resultado;
            }

            // Si hay varios héroes, buscar el más cercano
            int comp = -1;
            while (!enRango.empty()) {
                int idx = enRango.front();
                enRango.pop();

                int dx = heroes[idx]->getPosX() - monstruoPosX;
                int dy = heroes[idx]->getPosY() - monstruoPosY;
                int modulo = dx * dx + dy * dy;

                os << "  [H#" << idx+1 << "] Dist² = " << modulo << endl;

                if (comp == -1 || modulo < comp) {
                    comp = modulo;
                    resultado.posicion = {heroes[idx]->getPosX(), heroes[idx]->getPosY()};
                    resultado.idHeroe = idx;
                    os << "    -> Nuevo héroe más cercano: H#" << idx+1 
                    << " (Dist² = " << modulo << ")" << endl;
                }
            }

            os << "     Héroe más cercano final: H#" << resultado.idHeroe+1
            << " en (" << resultado.posicion.first << "," << resultado.posicion.second
            << ") con distancia² = " << comp << "." << endl;

            logLine(os.str(), false);
            return resultado;
        }

        bool enRango(const pair<int,int>& objetivo) {
            lock_guard<mutex> lock(mtx_grid); // protege lectura simultánea del grid

            int dx = objetivo.first - monstruoPosX;
            int dy = objetivo.second - monstruoPosY;

            double distancia = sqrt(dx*dx + dy*dy);

            bool dentro = (distancia <= monstruoRangoAtaque);

            ostringstream os;
            os << "Verificando rango: objetivo=(" << objetivo.first << "," << objetivo.second << ") "
            << "monstruo=(" << monstruoPosX << "," << monstruoPosY << ") "
            << "distancia=" << fixed << setprecision(2) << distancia 
            << " rango=" << monstruoRangoAtaque
            << " -> " << (dentro ? "DENTRO" : "FUERA");
            logLine(os.str(), false);

            return dentro;
        }


        void alertar(pair<int,int> ubi_heroe, const vector<Monstruo*>& monstruos) {
            int lim_x = columnas;
            int lim_y = filas;
            vector<int> lim_xy = limites(lim_x, lim_y);

            int X_low  = lim_xy[0];
            int X_high = lim_xy[1];
            int Y_low  = lim_xy[2];
            int Y_high = lim_xy[3];

            // solo bloquear lectura del grid para calcular vecinos
            vector<int> vecinos;
            {
                lock_guard<mutex> lock(mtx_grid);
                for (size_t i = 0; i < monstruos.size(); i++) {
                    if ((int)i != id &&
                        monstruos[i]->getVida() > 0 &&
                        (X_low <= monstruos[i]->getPosX() && X_high >= monstruos[i]->getPosX()) &&
                        (Y_low <= monstruos[i]->getPosY() && Y_high >= monstruos[i]->getPosY())) {
                        vecinos.push_back(i);
                    }
                }
            }

            // fuera del lock: permitir que otros hilos usen el grid
            for (int idx : vecinos) {
                ostringstream os;
                os << "Monstruo "<< id+1 << " tratará de avisar a monstruo " << (idx+1) << ".";
                logLine(os.str(), false);
                monstruos[idx]->logLine(os.str(), false);

                if (monstruos[idx]->lastQ() == pair<int,int> {-1,-1}) {
                    ostringstream os1;
                    os1 << "La queue del monstruo " << (idx+1) << " está vacia.";
                    logLine(os1.str(), false);
                    //aquí se usa mtx_pasos, arriba tambien para comprobar si está vacia la cola.
                    if (monstruos[idx]->genPasosHacia(ubi_heroe)) {
                        ostringstream os2;
                        os2 << "Avisó a monstruo " << (idx+1)
                        << ": de heroe en (" << ubi_heroe.first << "," << ubi_heroe.second << ").";
                        logLine(os2.str(), false);

                        ostringstream os3;
                        os3 << "Monstruo " << id+1 << " le avisó a monstruo " << (idx+1)
                            << ": de un heroe en (" << ubi_heroe.first << "," << ubi_heroe.second << ").";
                        monstruos[idx]->logLine(os3.str(), false);
                    }
                }
            }
        }


        void atacar(Heroe* &h){
            //bloquea el mutex hasta que el proceso atacar termine
            lock_guard<mutex> lock(mtx_combate);
            //revisa si tanto el heroe como elx monstruo están vivos
            if(monstruoVida>0 && h->getVida()>0){
                h->daniar(monstruoDanioAtaque);
                danioHecho+=monstruoDanioAtaque;

                {
                    ostringstream os;
                    os << "Atacó al héroe " << (h->getID()+1) 
                    << " en (" << h->getPosX() << ", " << h->getPosY() << "),"
                    << " desde (" << monstruoPosX << ", " << monstruoPosY << "),"
                    << " con " << monstruoRangoAtaque << " de rango de visión"
                    << " y le quitó " << monstruoDanioAtaque << " de vida.";
                    logLine(os.str(), false);
                }

                if(h->getVida() > 0){
                    ostringstream os;
                    os << "Vida restante del héroe " << (h->getID()+1) << ": " << h->getVida();
                    logLine(os.str(), false);
                } else {
                    ostringstream os;
                    os << "Mató al héroe " << (h->getID()+1) << ".";
                    logLine(os.str(), false);
                }
            }
            this_thread::sleep_for(0.000001s);
        }

        //se asume que no se topará a ningun heroe ya que esto va despues de la detección
        bool moverse(const vector<Monstruo*>& monstruos) {
            lock_guard<mutex> lock(mtx_pasos);
            if (pasosSgtes.empty()) return false;

            auto sgt = pasosSgtes.front();

            // Verificar límites del grid
            if (sgt.first < 0 || sgt.second < 0 || 
                sgt.first >= columnas || sgt.second >= filas) {
                logLine("Coordenada fuera de rango: (" + to_string(sgt.first) + "," + to_string(sgt.second) + ")", false);
                while (!pasosSgtes.empty()) pasosSgtes.pop();
                return false;
            }

            // Evitar moverse si el lugar está ocupado
            {
                lock_guard<mutex> lockG(mtx_grid);
                for (auto* m : monstruos) {
                    if (m != this && m->getPosX() == sgt.first && m->getPosY() == sgt.second && m->getVida()>0) {
                        logLine("Movimiento bloqueado: otro monstruo ocupa (" + to_string(sgt.first) + "," + to_string(sgt.second) + ")", false);
                        return false;
                    }
                }
            }

            monstruoPosX = sgt.first;
            monstruoPosY = sgt.second;
            pasosSgtes.pop();

            ostringstream os;
            os << "Se movió hacia (" << sgt.first << "," << sgt.second << ")";
            logLine(os.str(), false);

            return true;
        }


        void* loop_monstruo(vector<Heroe*> &heroes, vector<Monstruo*> &monstruos);
    };

bool simulacionTerminada(const vector<Heroe*>& heroes) {
    for (auto h : heroes){
        if (h->getVida() > 0 && !h->terminado())
        return false;
    }
    return true;
}



void imprimirGrid(const vector<Heroe*>& heroes, const vector<Monstruo*>& monstruos, bool idd) {
    if(borrar && !idd) { system("clear"); }
    cout << "Estado del GRID en T = " << tick << "." << endl;
    lock_guard<mutex> lock(mtx_grid);
    for(int i = 0 ; i <= filas ; i++){
        if(i != 0){
            if(i > 9){
                cout<<i<<" ";
            }
            else{
                cout<<i<<"  ";
            }
        }
        for(int j = 0 ; j <= columnas ; j++){
            if(i == 0 && j == 0){
                cout<<"    ";
                for(int c = 0 ; c <= columnas ; c++){
                    if(c<10){
                        cout<<"  "<<c<<"  ";
                    }
                    else{
                        cout<<"  "<<c<<" ";
                    }
                }
                cout<<endl;
                cout<<i<<"  ";
            }

            if(j==0){
                cout <<"|";
            }
            
            bool hayAlguien = false;

            // héroes
            for (size_t k = 0; k < heroes.size(); k++) {
                if (i == heroes[k]->getPosY() && j == heroes[k]->getPosX() && heroes[k]->getVida() > 0) {

                    int num = (idd == false) ? heroes[k]->getVida() : heroes[k]->getID() + 1;
                    string displayed;
                    if(num - 100 < 0){
                        displayed+=" ";
                    }

                    displayed += to_string(num);

                    if(!color_consola){
                        displayed+="⁺";
                    }
                    else{
                        displayed+=" ";
                    }
                    

                    if(num - 10 < 0){
                        displayed+=" ";
                    }

                    if (heroes[k]->terminado()) {
                        cout << "\033[32m" << displayed << "\033[0m";   // verde = terminó
                    }
                    else if (heroes[k]->getCombatiendo()) {
                        cout << "\033[1;34m" << displayed << "\033[0m"; // azul = combatiendo
                    }
                    else {
                        cout << "\033[1;37m" << displayed << "\033[0m"; // blanco = moviendose
                    }
                    hayAlguien = true;
                    break;
                }
            }

            // monstruos
            if(!hayAlguien){
                for(size_t k = 0 ; k < monstruos.size() ; k++){
                    if(i==monstruos[k]->getPosY() && j==monstruos[k]->getPosX() && monstruos[k]->getVida() > 0){
                        int num = (idd == false) ? monstruos[k]->getVida() : monstruos[k]->getID() + 1;
                        string displayed;

                        if(num - 100 < 0){
                            displayed+=" ";
                        }

                        displayed+= to_string(num);

                        if(!color_consola){
                            displayed+="⁻";
                        }
                        else{
                            displayed+=" ";
                        }

                        if(num - 10 < 0){
                            displayed+=" ";
                        }

                        if (monstruos[k]->getSiguiendo()) {
                            cout << "\033[31m" << displayed << "\033[0m"; // rojo = siguiendo
                        }
                        else if (monstruos[k]->getCombatiendo()) {
                            cout << "\033[35m" << displayed << "\033[0m"; // morado = combatiendo
                        }
                        else{
                            cout << "\033[33m" << displayed << "\033[0m"; // naranjo = pasivo
                        }
                        hayAlguien=true;
                        break;
                    }
                }
            }

            if(!hayAlguien){
                cout<< " -- ";
            }

            cout <<"|";
        }
        cout<<endl;
    }
}

void* Monstruo:: loop_monstruo(vector<Heroe*>& heroes, vector<Monstruo*> &monstruos){
    while (heroes_vivos>0 && !simulacionTerminada(heroes) && monstruoVida>0)
    {
        loopExec++;
        combatiendo=false;
        siguiendo=false;

        Objetivo heroe_mas_cercano = detectarMasCercano(heroes);
        int cont=0;
        //si cualquiera de las coordenadas es negativa no pasa nada (Estado Pasivo, heroe no encontrado)
        if(heroe_mas_cercano.posicion.first >= 0 && heroe_mas_cercano.posicion.second >= 0){
            // Marcamos qué héroe detectó
            {
                ostringstream os;
                os << "Detectó héroe " << (heroe_mas_cercano.idHeroe+1)
                   << " en (" << heroe_mas_cercano.posicion.first << ","
                   << heroe_mas_cercano.posicion.second << ").";
                logLine(os.str(), false);
            }

            alertar(heroe_mas_cercano.posicion,monstruos);
            
            //si está vacia o si es de mayor distancia que el heroe detectado.
            if (lastQ() != pair<int,int>{-1, -1} || 
                sqrt(pow(lastQ().first - monstruoPosX, 2) + pow(lastQ().second - monstruoPosY, 2))
                >
                sqrt(pow(heroe_mas_cercano.posicion.first - monstruoPosX, 2) + pow(heroe_mas_cercano.posicion.second - monstruoPosY, 2))) 
            {
                logLine("path vacio",false);
                genPasosHacia(heroe_mas_cercano.posicion);
            }

            if (enRango(heroe_mas_cercano.posicion)) {
                combatiendo=true;
                ostringstream os;
                os << "ATACANDO A HEROE " << heroe_mas_cercano.idHeroe+1 << ".";
                logLine(os.str(),false);
                atacar(heroes[heroe_mas_cercano.idHeroe]);
            } else {
                if(enRango(heroe_mas_cercano.posicion)){
                    ostringstream os;
                    os << "EL HEROE " << heroe_mas_cercano.idHeroe+1 << " ESTABA FUERA DE RANGO DE ATAQUE.";
                    logLine(os.str(),false);
                }
                siguiendo=true;
                moverse(monstruos);
            }
            
            cont++;
        }

        if(lastQ() != pair<int,int>{-1, -1} && cont==0){
            siguiendo=true;
            moverse(monstruos);
            cont++;
        }
        
        //si no pasa nada, que se vea en el grid.
        if(cont==0){
            combatiendo=false;
            siguiendo=false;
            logLine("En estado pasivo (sin detección ni movimientos).", false);
        }
        
        this_thread::sleep_for(tfinal);
    }
    return nullptr;
}


int Heroe::detectar_enRango(const vector<Monstruo*>& monstruos) {
    int resultado = -1;

    vector<int> lim = limites(columnas, filas);
    int X_low = lim[0], X_high = lim[1];
    int Y_low = lim[2], Y_high = lim[3];

    int mejorDist = INT_MAX;
    int cont = 0;

    

    {
        //lock al grid para escanear.
        lock_guard<mutex> lock(mtx_grid);
        ostringstream os;
        os << "\n[HEROE " << id+1 << "] Escaneando monstruos en rango: "
         << "X[" << X_low << "," << X_high << "], "
         << "Y[" << Y_low << "," << Y_high << "] desde ("
         << heroePosX << "," << heroePosY << ")" << endl;

        for (size_t i = 0; i < monstruos.size(); i++) {
            Monstruo* m = monstruos[i];
            if (!m) continue;

            int mx = m->getPosX();
            int my = m->getPosY();
            int vida = m->getVida();

            if (vida <= 0) {
                continue;
            }

            if (mx < X_low || mx > X_high || my < Y_low || my > Y_high) {
                continue;
            }

            int dist = (mx - heroePosX)*(mx - heroePosX) + 
                    (my - heroePosY)*(my - heroePosY);

            os << "  [M#" << i+1 << "] detectado en (" 
                << mx << "," << my << ") — Dist² = " << dist << endl;

            if (dist < mejorDist || cont == 0) {
                mejorDist = dist;
                resultado = (int)i;
                cont++;
                os << "    -> Nuevo monstruo más cercano: M#" 
                    << i+1 << " (Dist² = " << mejorDist << ")" << endl;
            }
        }
    

        if (resultado == -1){
            os << "      Ningún monstruo detectado en el rango." << endl;
        }  
        else{
            os << "      Monstruo más cercano: M#" << resultado+1 
                << " a distancia² = " << mejorDist << "." << endl;
        }
        logLine(os.str(),false);  
    }
    return resultado;
}

void Heroe::atacar(Monstruo* &m){
    lock_guard<mutex> lock(mtx_combate);
    ostringstream os;
    if(heroeVida > 0 && m && m->getVida() > 0){
        m->daniar(heroeDanioAtaque);
        os << "El heroe " << id+1 << " ataco al monstruo " << m->getID()+1
        << " y le quito " << heroeDanioAtaque << " de vida." << endl;

        if(m->getVida() <= 0){
            os << "El monstruo " << m->getID()+1
            << " ha muerto a manos del heroe " << id+1 << "." << endl;
        }
        else{
            os << "Vida restante del monstruo " << m->getID()+1 << ": " << m->getVida() << endl;
        }
    }
    this_thread::sleep_for(0.0000009s);//pequeño buff a heroes
    logLine(os.str(),false);
}

void* Heroe::loop_heroe(vector<Heroe*>& heroes, vector<Monstruo*>& monstruos){
    while(paso + 1 < (int)heroePath.size() && heroeVida > 0){
        int id_monstruo_cercano = detectar_enRango(monstruos);
        if(id_monstruo_cercano != -1){
            atacar(monstruos[id_monstruo_cercano]);
            combatiendo = true;
        } else {
            combatiendo = false;
            avanzar(heroes);
        }
        this_thread::sleep_for(tfinal);
    }
    ostringstream os;
    os << "El heroe " << id+1 << " ha terminado su recorrido." << endl;
    logLine(os.str(),false);
    return nullptr;
}

void limpiarLogs(int cantHeroes, int cantMonstruos) {
    const string dirHeroe = "log_heroes";
    const string dirMonstruo = "log_monstruos";

    int eliminados = 0;
    

    try {
        // === HEROES ===
        if (fs::exists(dirHeroe)) {
            int basta = 0;
            for (int i = 1; i <= cantHeroes; ++i) {
                string path = dirHeroe + "/heroe_" + to_string(i) + ".log";
                if (fs::exists(path)) {
                    fs::remove(path);
                    eliminados++;
                }
                else{
                    basta++;
                }
                if (basta > 5){
                    break;
                }
            }
        }

        // === MONSTRUOS ===
        if (fs::exists(dirMonstruo)) {
            int basta = 0;
            for (int i = 1; i <= cantMonstruos; ++i) {
                string path = dirMonstruo + "/monstruo_" + to_string(i) + ".log";
                if (fs::exists(path)) {
                    fs::remove(path);
                    eliminados++;
                }
                else{
                    basta++;
                }
                if (basta > 5){
                    break;
                }
            }
        }

        cout << "\n\033[1;33mLogs eliminados correctamente:\033[0m \n" 
             << eliminados << " archivos.\n";
    }
    catch (const fs::filesystem_error& e) {
        cerr << "\n\033[1;31mError al limpiar logs:\033[0m \n" << e.what() << '\n';
    }
}
        
int main(){
    // se limpian logs con un max de 100, pero cuando falten 5 se detiene solo
    limpiarLogs(100, 100);

    // para lectura del archivo
    ifstream archivo;
    string etiqueta;

    // variables para el/los heroes
    queue<int> heroeVida, heroeDanioAtaque, heroeRangoAtaque;
    vector<Heroe*> heroes;
    queue<int> heroePosX;
    queue<int> heroePosY; 
    // se asume que las posiciones y el path serán coherentes
    //  ej. si el heroe parte en (2,5)
    //  el path parte en (3,5) (a 1 "cuadro" de distancia).
    queue <vector<pair<int,int>>> heroePath;

    // variables para los monstruos
    vector<Monstruo*> monstruos;
    queue<int> monstruoVida;
    queue<int> monstruoDanioAtaque;
    queue<int> monstruoRangoAtaque;
    queue<int> monstruoRangoVision;
    queue<int> monstruoPosX;
    queue<int> monstruoPosY;

    int contM=0;
    int archivoM;
    int bul;

    while(true){
        if(contM==0){
            cout << "\n==== MENÚ INICIAL ====" << endl;
            cout << "\nElegir tamaño del grid y cantidad de Heroes y Monstruos." << endl;
            cout << " (Elegir simulacion)" << endl;
            cout << "   1 = ejemplo 1, el propuesto en modulos, pero adaptado a etiquetas del este codigo" << endl;
            cout << "   2 = ejemplo 2, el propuesto en modulos, pero adaptado a etiquetas del este codigo" << endl;
            cout << "   3 = ejemplo 3, el propuesto en modulos, pero adaptado a etiquetas del este codigo" << endl;
            cout << "   4 = 15x15 con 9 heroe y 17 monstruos." << endl;
            cout << "   5 = 30x30 con 15 heroes y 30 monstruos.\n" << endl;
        }
        else if(contM==1){
            //color_consola, declarado arriba
            cout << "\n\n Su consola tiene mas de 6 colores diferenciables/responde a los codigos ANSI?" << endl;
            cout << "   Si => diferenciación de heroes y monstruos por color\n   No => se diferenciaran heroes y monstruos con ⁺ (heroe) y ⁻ (monstruo)\n" << endl;
        }
        else if(contM==2){
            cout << "\n\nCuanto tiempo quiere que haya entre ciclos?\n" << endl; 
        }
        else if(contM==3){
            cout << "\n\nQuiere que el grid se mantenga o se borre en cada ciclo (multiples grids o solo uno)\n" << endl;
        }

        if (contM==0){
            cout << "Respuesta (1, 2, 3, 4 o 5): ";
            cin >> archivoM;
            if(archivoM < 1 || archivoM > 5){
                cout << "\npa q po :)))))\n" << endl;
            }
            else{
                contM++;
            }
        }
        else if (contM==1){
            cout << "Respuesta 1 = true/si, 0 = false/no): ";
            cin >> bul;
            if(bul == 0 || bul == 1){
                //creo q si hago bul = color_consola o hago de eso el input es lo mismo, pero para q si puedo aplicar mis grandes conocimientos de progra 1 (literalmente enseñaron eso y se me olvidó.)
                if(!bul){
                    color_consola=false;
                }
                contM++;
            }
        }
        else if (contM==2){
            cout << "Respuesta en segundos (ej. 0.5, 0.25, 1, etc.) tiempo entre ciclos (se recomienda entre 0.2 y 1): ";
            cin >> tiempo;
            if(tiempo > 0.0){
                tfinal = duration<double> (tiempo);
                contM++;
            }
            else{
                cout << "\npa q po :)))))\n" << endl;
            }
        }
        else if (contM==3){
            cout << "Respuesta 1 = true/si se borra, 0 = false/no se borra): ";
            cin >> borrar;
            if(borrar == 0 || borrar == 1){
                // probando...
                contM++;
            }

        }
        else if (contM > 3){
            cout << "Recomiendo tener la consola en pantalla completa para las simulaciones grandes" << endl;
            cout << "Presione ENTER para iniciar..." << endl;
            cout << "\n====== GUIA DE COLORES ======" << endl;
            cout << "  HEROES:\n";
            cout << "    \033[37mBlanco\033[0m: Siguen su path.\n";
            cout << "    \033[34mAzul\033[0m: Combatiendo.\n";
            cout << "    \033[32mVerde\033[0m: Finalizaron el path (inmortales e indetectables).\n\n";
            
            cout << "  MONSTRUOS:\n";
            cout << "    \033[33mNaranjo\033[0m: Estado Pasivo.\n";
            cout << "    \033[31mRojo\033[0m: Siguiendo a heroes.\n";
            cout << "    \033[35mMorado\033[0m: Combatiendo.\n";
            cout << "El grid muestra la vida de cada heroe y monstruo.\nAl final se muestran las ID por se quieren revisar los logs (se ubican en la carpeta Heroe o Monstruo respectivamente)." << endl;
            cin.ignore();   
            cin.get();      
            break;// sale del bucle
        }
    }

    if(archivoM==1){
        archivo.open("./ejemplo_1.txt");
    }
    else if(archivoM==2){
        archivo.open("./ejemplo_2.txt");
    }
    else if(archivoM==3){
        archivo.open("./ejemplo_3.txt"); 
    }
    else if(archivoM==4){
        archivo.open("./15x15.txt");
    }
    else if(archivoM==5){
        archivo.open("./30x30.txt");
    }

    if(!archivo.is_open()){
        cout << "No se pudo abrir el archivo" << endl;
    }
    else{
        while (archivo >> etiqueta) {

            if (etiqueta == "heroeVida:") {
                int x;
                archivo >> x;
                heroeVida.push(x);
            }
            else if (etiqueta == "heroeDanioAtaque:") {
                int x;
                archivo >> x;
                heroeDanioAtaque.push(x);
            }
            else if (etiqueta == "heroeRangoAtaque:") {
                int x;
                archivo >> x;
                heroeRangoAtaque.push(x);
            }
            else if (etiqueta == "heroePosX:") {
                int x;
                archivo >> x;
                heroePosX.push(x);
            }
            else if (etiqueta == "heroePosY:") {
                int y;
                archivo >> y;
                heroePosY.push(y);
            }
            else if (etiqueta == "fila_max:") {
                    archivo >> filas; //si filas=10, coordenadas desde (x,0) a (x,10).
                
            }
            else if (etiqueta == "columna_max:") {
                    archivo >> columnas; //si columnas=10, coordenadas desde (0,y) a (10,y).
            }
            else if (etiqueta == "monstruoVida:") {
                int v;
                archivo >> v;
                monstruoVida.push(v);
            }
            else if (etiqueta == "monstruoDanioAtaque:") {
                int da;
                archivo >> da;
                monstruoDanioAtaque.push(da);
            }
            else if (etiqueta == "monstruoRangoAtaque:") {
                int ra;
                archivo >> ra;
                monstruoRangoAtaque.push(ra);
            }
            else if (etiqueta == "monstruoRangoVision:") {
                int rv;
                archivo >> rv;
                monstruoRangoVision.push(rv);
            }
            else if (etiqueta == "monstruoPosX:") {
                int mx;
                archivo >> mx;
                monstruoPosX.push(mx);
            }
            else if (etiqueta == "monstruoPosY:") {
                int my;
                archivo >> my;
                monstruoPosY.push(my);
            }
            else if (etiqueta == "heroePath:") {
                // leer TODO lo que queda en esa línea, ej:
                // " (3,4)(4,4)(4,3)(5,3)..."
                string lineaRestante;
                getline(archivo, lineaRestante);

                // vector de pasos para ESTE héroe
                vector<pair<int,int>> caminoActual;

                size_t i = 0;
                while (i < lineaRestante.size()) {

                    if (lineaRestante[i] == '(') {
                        i++; // saltar '('

                        // leer numero X (solo dígitos)
                        int x = 0;
                        while (i < lineaRestante.size() && isdigit(lineaRestante[i])) {
                            x = x * 10 + (lineaRestante[i] - '0'); // construye número
                            i++;
                        }

                        // saltar la coma
                        if (i < lineaRestante.size() && lineaRestante[i] == ',') {
                            i++;
                        }

                        // leer numero Y (solo dígitos)
                        int y = 0;
                        while (i < lineaRestante.size() && isdigit(lineaRestante[i])) {
                            y = y * 10 + (lineaRestante[i] - '0');
                            i++;
                        }

                        // saltar ')'
                        if (i < lineaRestante.size() && lineaRestante[i] == ')') {
                            i++;
                        }

                        // guardar el paso (x,y) en el vector del héroe actual
                        caminoActual.push_back({x, y});
                    }
                    else {
                        // ignorar cualquier caracter que no sea '('
                        i++;
                    }
                }

                // al terminar la línea, agregamos el camino del héroe a la cola global
                heroePath.push(caminoActual);
            }
            else {
                cout << "Etiqueta no reconocida: " << etiqueta << endl;
            }
        }
    }

    //crear heroes
    while (!heroePosX.empty())
    {
        Heroe *h = new Heroe(heroeVida.front(), heroeDanioAtaque.front(), heroeRangoAtaque.front(), heroePosX.front(), heroePosY.front(), heroePath.front());
        heroes.push_back(h);
        heroePosX.pop();
        heroePosY.pop();
        heroePath.pop();
        //aqui no habrán cambios asi que no hay mutex.
        heroes_vivos++;
    }

    //estados iniciales de heroes (comprobación de funcionamiento)
    for(size_t i=0; i<heroes.size(); i++){
        heroes[i]->mostrarEstado();
    }
    
    //crear monstruos
    while(!monstruoVida.empty()){
        Monstruo *m = new Monstruo(monstruoVida.front(), monstruoDanioAtaque.front(), monstruoRangoAtaque.front(), monstruoPosX.front(), monstruoPosY.front(), monstruoRangoVision.front());
        monstruos.push_back(m);
        monstruoVida.pop();
        monstruoDanioAtaque.pop();
        monstruoRangoAtaque.pop();
        monstruoRangoVision.pop();
        monstruoPosX.pop();
        monstruoPosY.pop();
    }

    //estados iniciales de monstruos (comprobación de funcionamiento)
    for(size_t i=0; i<monstruos.size(); i++){
        monstruos[i]->monstrarEstado();
    }
    

    vector<thread> hilos;

    size_t i_heroe = 0;
    size_t i_monstruo = 0;
    size_t total = heroes.size() + monstruos.size();

    {
        lock_guard<mutex> lock(mtx_combate);
        {
            lock_guard<mutex> lock2(mtx_grid);
            for (size_t i = 0; i < total; i++) {
                // pares -> héroe, impares -> monstruo
                if (i % 2 == 0 && i_heroe < heroes.size()) {
                    hilos.emplace_back([&, i_heroe]() {
                        heroes[i_heroe]->loop_heroe(heroes, monstruos);
                    });
                    i_heroe++;
                } else if (i_monstruo < monstruos.size()) {
                    hilos.emplace_back([&, i_monstruo]() {
                        monstruos[i_monstruo]->loop_monstruo(heroes, monstruos);
                    });
                    i_monstruo++;
                }
            }
        }
    }
    while (!simulacionTerminada(heroes) && heroes_vivos > 0) {
        cout<<endl;
        imprimirGrid(heroes, monstruos, false);
        tick++;
        this_thread::sleep_for(tfinal/2);
    }
    this_thread::sleep_for(0.1s);
    cout << endl;
    cout << "GRIDS FINALES" << endl;
    imprimirGrid(heroes, monstruos, false);
    imprimirGrid(heroes, monstruos, true);
    cout << "Lo que se ve son los id de heroes y monstruos, por si quiere ver los logs de cada uno. \nArriba se puede ver la vida restante de cada entidad (scroll up). \nCompasión con la nota C:\n\nCristobal Navarro y Maria de los Angeles Martinez." << endl;

    //estado final heroes
    for(size_t i=0; i<heroes.size(); i++){
        heroes[i]->mostrarEstado();
    }

    //estado final monstruos
    for(size_t i=0; i<monstruos.size(); i++){
        monstruos[i]->monstrarEstado();
    }

    // esperar a que terminen todos
    for (auto& h : hilos) {
        if (h.joinable()) h.join();
    }
}