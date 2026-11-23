/*
  MIDI XY CONTROLLER - Capacitive Minimal
  Arduino Nano + 4 pines Dupont sobre PCB perforada + ADCTouch

  Conexiones:
    A0 = Pad superior izquierdo
    A1 = Pad superior derecho
    A2 = Pad inferior izquierdo
    A3 = Pad inferior derecho
    GND = Guarda / barra de masa (tocar con la palma)

  Mapeo MIDI:
    Eje X  -> notas (escala pentatonica de Do)
    Eje Y  -> CC74 (filtro)
    Presion -> CC11 (expresion) y velocity

  IMPORTANTE:
    Sin referencia a GND, los pads son antenas y el ruido de red
    domina la lectura. Poner cinta de cobre en cada pad (1x1 a 2x2 cm)
    y una guarda a GND alrededor. Ver README.

  Baud rate: 115200
  Canal MIDI: 1
*/

#include <ADCTouch.h>

const byte pads[4] = {A0, A1, A2, A3};
const byte CANAL = 1;

const byte notas[] = {60, 62, 64, 67, 69, 72, 74, 76};
const byte cantidadNotas = sizeof(notas) / sizeof(notas[0]);

// Muestras por lectura. Subir a 70 si el CC74 tiembla.
const int MUESTRAS = 50;

// Lecturas consecutivas por encima del umbral para aceptar el toque
const byte TOQUE_SOSTENIDO = 4;

float baseline[4];
float ganancia[4];
float umbralON  = 40.0;
float umbralOFF = 20.0;

bool  tocando       = false;
byte  contadorToque = 0;
int   notaActual    = -1;
float xSuave = 0.5, ySuave = 0.5;
float xMin = 0.15, xMax = 0.85, yMin = 0.15, yMax = 0.85;
unsigned long tBaseline = 0;

// ---------- MIDI ----------
void notaON(byte n, byte v){
  Serial.write(0x90 | (CANAL - 1));
  Serial.write(n);
  Serial.write(v);
}

void notaOFF(byte n){
  Serial.write(0x80 | (CANAL - 1));
  Serial.write(n);
  Serial.write((byte)0);
}

void enviarCC(byte c, byte v){
  Serial.write(0xB0 | (CANAL - 1));
  Serial.write(c);
  Serial.write(v);
}

// ---------- Lectura con descarte ----------
// La primera lectura limpia el ADC; la segunda es la real.
float leerPad(byte i){
  ADCTouch.read(pads[i], MUESTRAS);
  return ADCTouch.read(pads[i], MUESTRAS);
}

// ---------- Mediana de 3 ----------
float mediana3(float a, float b, float c){
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

// ---------- Calibracion ----------
void calibrar(){
  Serial.println(F("== CALIBRACION =="));
  Serial.println(F("NO toques. Sin tocar GND tampoco."));

  // 1) Baseline inicial
  for (byte i = 0; i < 4; i++){
    float s = 0;
    for (byte k = 0; k < 10; k++) s += leerPad(i);
    baseline[i] = s / 10.0;
  }

  // 2) Ganancia por pad: tocar cada uno 1.2 s CON GND tocado
  Serial.println(F("Toca cada pad 1s, en orden A0 A1 A2 A3. CON GND tocado."));
  for (byte i = 0; i < 4; i++){
    Serial.print(F("Pad ")); Serial.print(i); Serial.println(F(" -> toca ya"));
    float m = 0;
    unsigned long t0 = millis();
    while (millis() - t0 < 1200){
      float d = leerPad(i) - baseline[i];
      if (d > m) m = d;
    }
    if (m < 5.0){
      ganancia[i] = 1.0;
      Serial.println(F("  muy baja, pad muerto o mal soldado"));
    } else {
      ganancia[i] = 100.0 / m;
      Serial.print(F("  max=")); Serial.print(m);
      Serial.print(F(" gan=")); Serial.println(ganancia[i]);
    }
  }

  // 3) Soltar todo y estabilizar (5 s)
  Serial.println(F("SUELTA todo. Ni pads ni GND."));
  for (byte k = 0; k < 100; k++){
    for (byte i = 0; i < 4; i++) leerPad(i);
    delay(50);
  }

  // 4) Re-baseline tras el toque
  for (byte i = 0; i < 4; i++){
    float s = 0;
    for (byte k = 0; k < 15; k++) s += leerPad(i);
    baseline[i] = s / 15.0;
  }

  // 5) Ruido promedio sobre baseline fresca
  float ruidoProm = 0, ruidoMax = 0;
  int n = 0;
  for (byte k = 0; k < 60; k++){
    for (byte i = 0; i < 4; i++){
      float d = (leerPad(i) - baseline[i]) * ganancia[i];
      if (d < 0) d = -d;
      if (d > ruidoMax) ruidoMax = d;
      ruidoProm += d;
      n++;
    }
    delay(20);
  }
  ruidoProm /= n;

  umbralON  = ruidoProm * 8.0;
  if (umbralON < 40.0)  umbralON = 40.0;
  if (umbralON > 100.0) umbralON = 100.0;
  umbralOFF = umbralON * 0.6;

  Serial.print(F("Ruido prom=")); Serial.print(ruidoProm);
  Serial.print(F(" max="));      Serial.print(ruidoMax);
  Serial.print(F(" ON="));       Serial.print(umbralON);
  Serial.print(F(" OFF="));      Serial.println(umbralOFF);

  // 6) Verificacion en reposo (4 s)
  Serial.println(F("Verifica: NO toques nada. Si suma>ON, el hardware no da."));
  unsigned long t0 = millis();
  float sumaMaxVer = 0;
  while (millis() - t0 < 4000){
    float suma = 0;
    for (byte i = 0; i < 4; i++){
      float d = (leerPad(i) - baseline[i]) * ganancia[i];
      if (d < 0) d = 0;
      suma += d;
    }
    if (suma > sumaMaxVer) sumaMaxVer = suma;
    delay(200);
  }
  Serial.print(F("Max suma en reposo=")); Serial.println(sumaMaxVer);

  if (sumaMaxVer > umbralON * 0.5){
    Serial.println(F("!! AVISO: ruido comparable al umbral."));
    Serial.println(F("!! Necesitas cobre en pads y guarda a GND."));
  }
  Serial.println(F("Listo."));
}

void setup(){
  Serial.begin(115200);
  delay(1500);
  calibrar();
}

void loop(){
  float v[4];
  float suma = 0;

  for (byte i = 0; i < 4; i++){
    float d = leerPad(i) - baseline[i];
    if (d < 0) d = 0;
    v[i] = d * ganancia[i];
    suma += v[i];
  }

  // Baseline adaptativo: solo cuando no estamos tocando
  if (!tocando && millis() - tBaseline > 500){
    tBaseline = millis();
    for (byte i = 0; i < 4; i++){
      if (v[i] < umbralON * 0.2){
        baseline[i] = baseline[i] * 0.9 + leerPad(i) * 0.1;
      }
    }
  }

  // Filtro de mediana sobre la suma
  static float hist[3] = {0, 0, 0};
  hist[0] = hist[1];
  hist[1] = hist[2];
  hist[2] = suma;
  float sumaF = mediana3(hist[0], hist[1], hist[2]);

  // Deteccion de toque con sostenido
  if (!tocando){
    if (sumaF > umbralON){
      if (++contadorToque >= TOQUE_SOSTENIDO){
        tocando = true;
        contadorToque = 0;
        xSuave = 0.5;
        ySuave = 0.5;
        notaActual = -1;
      }
    } else {
      contadorToque = 0;
    }
  } else {
    if (sumaF < umbralOFF){
      tocando = false;
      if (notaActual >= 0){
        notaOFF(notaActual);
        notaActual = -1;
      }
      enviarCC(11, 0);
    }
  }

  // Mapeo XY -> MIDI
  if (tocando){
    float x = (v[1] + v[3]) / suma;
    float y = (v[0] + v[1]) / suma;

    xSuave += (x - xSuave) * 0.15;
    ySuave += (y - ySuave) * 0.15;

    float xn = constrain((xSuave - xMin) / (xMax - xMin), 0.0, 1.0);
    float yn = constrain((ySuave - yMin) / (yMax - yMin), 0.0, 1.0);

    int idx = constrain((int)(xn * cantidadNotas), 0, cantidadNotas - 1);
    if (notaActual != notas[idx]){
      if (notaActual >= 0) notaOFF(notaActual);
      notaActual = notas[idx];
      notaON(notaActual, constrain((int)suma, 40, 127));
    }

    enviarCC(74, (byte)(yn * 127));
    enviarCC(11, (byte)constrain((int)suma, 40, 127));
  }

  delay(5);
}