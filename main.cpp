//******* 9. Création de tâches *******\\
// #include <M5Unified.h>
// #undef min  // https://github.com/m5stack/M5Stack/issues/97
// #include <iostream>
// #include <regex>
// #include <string>

// #define TRAME_SIZE  40


// typedef struct 
// {
//     uint8_t numLigne;
//     String text;
// }t_message_lcd;


// // Déclaration des fonctions
// void Serial_callback();
// void taskTraiteTrame(void *pvParameters);
// void taskGestionLcd(void *pvParameters);

// // Déclration des variables globales
// static QueueHandle_t queueReceptionSerie;
// static QueueHandle_t queueAffichage;

// // SETUP *****************************************************************
// void setup() {
//     M5.begin(); // Initialise le port série du M5Stack à 115200 bauds
//     Serial.begin(115200);

//     // Initialisation d'une file de 40 char
//     queueReceptionSerie = xQueueCreate(TRAME_SIZE, sizeof(char));
//     queueAffichage = xQueueCreate(TRAME_SIZE, sizeof(t_message_lcd));

//     xTaskCreatePinnedToCore(taskTraiteTrame,    // Function
//                           "traiteTrame",        // Name
//                           8192,        // Stack size
//                           nullptr,     // Parameters
//                           2,           // Priority
//                           nullptr,     // Task handle
//                           1);          // Core

//     xTaskCreatePinnedToCore(taskGestionLcd,    // Function
//                           "gestionLcd",        // Name
//                           8192,        // Stack size
//                           nullptr,     // Parameters
//                           2,           // Priority
//                           nullptr,     // Task handle
//                           1);          // Core
                          
//     // Affectation de la fonction de callBack à l'évènement OnReceive
//     // du port série
//     Serial.onReceive(Serial_callback); 
// } 

// // LOOP ******************************************************************
// void loop() {  
//     // Tout se fait dans les tâches et/ou callback
// }

// /**************************************************************************
//  * Lit les caractères reçus sur le port série et les ajoute dans 
//  * la file "queueReceptionSerie"
//  */
// void Serial_callback() {
//   char c;
//   while(Serial.available() > 0) {   // Si au moins 1 caractère reçu
//     Serial.read(&c, 1);             // Le lire
//     xQueueSendToBack(queueReceptionSerie, (void *)&c, portMAX_DELAY);    // L'envoyer dans la file
//   }
// }

// /**************************************************************************
//  * Tâche de traitement de la trame :
//  *      Trames vides (contenant juste \n et/ou \r) ignorées 
//  *      Trames contenant juste "E" => echo
//  *      Trames match "L[1-3][ !?.,;0-9a-zA-Z]{1,}" => 
//  *          ToDo : Afficher sur la ligne 1, 2 ou 3 du LCD la partie [ !?.,;0-9a-zA-Z]{1,}
//  *                 en transmettant le message à une autre tâche par une file (queue)
//  */
// void taskTraiteTrame(void *pvParameters) {
    
//     char buffer[TRAME_SIZE];
//     t_message_lcd msg; // = {1, "Salut a tous c'est Lazagne"};
//     uint8_t i = 0;
    
//     Serial.println("taskTraiteTrame");

//     while(1) {
//         if(xQueueReceive(queueReceptionSerie, (void *)&buffer[i], 1)) { // On stock les caractères reçu dans "buffer"
//             if(buffer[i] == '\r' || buffer[i] == '\n') { // \r ou \n marquent la fin de la trame
//                 buffer[i] = '\0';
//                 if(buffer[0] == 'E' && i == 1) {  // Echo, la trame contient juste E
//                     Serial.println("E");
//                 } else if (buffer[0] == '\0') { // Ignore les trames vides qui contiennent juste \r ou \n
//                     i = 0;
//                 } else if (std::regex_match(std::string(buffer), std::regex("L[1-3][ !?.,;0-9a-zéèàêA-Z]{1,}"))) {
//                     Serial.printf("Ajout ligne %u : %s\n", buffer[1] - '0', &buffer[2]); // Renvoie de la trame décodée
//                     // ToDo pour les étudiants :
//                     //      Afficher sur l'écran LCD du M5 le texte reçu dans la trame
//                     //      sur la ligne x (x : 2ème caractère de la trame)
//                     msg.numLigne = buffer[1] - '0';
//                     msg.text = String(&buffer[2]);
//                     xQueueSend(queueAffichage, &msg, portMAX_DELAY);
//                 }
//                 else {
//                     Serial.println("ERROR");
//                 }
//                 i = 0;
//             } else {
//                 if(++i > TRAME_SIZE - 1) i=0; // Attention à ne pas dépasser 39 !
//             }
//         }
//     }
// }

// void taskGestionLcd(void *pvParameters){

//     t_message_lcd message ;//= *(t_message_lcd*)pvParameters;
//     M5.Lcd.setTextSize(3);
    
//     for (;;)
//     {
//         if (xQueueReceive(queueAffichage, &message, portMAX_DELAY))
//         {
//             M5.Lcd.setTextColor(0xFFFFFF);
//             M5.Lcd.fillScreen(TFT_BLACK);
//             M5.Lcd.setCursor((M5.Lcd.width() - M5.Lcd.textWidth(message.text)) / 2, message.numLigne * 50);
//             M5.Lcd.println(message.text);
//         }
        
//     }

// }



//******* 11. Sémaphore *******\\


#include <M5Unified.h>

#define PIN_POTENTIOMETRE   36      // GPIO 36 (entrée analogique uniquement)
#define TAILLE_MOYENNE      128     // Nombre d'échantillons de la moyenne glissante
#define NUMERO_TIMER        0       // Timer matériel utilisé
#define DIVISEUR_TIMER      80      // 80 MHz / 80 = 1 MHz (1 tick = 1 µs)
#define PERIODE_TIMER_US    2000    // 2000 µs = 2 ms
#define TAILLE_PILE_TACHE   8192
#define PRIORITE_TACHE      2
#define COEUR_TACHE         1

// Déclaration des fonctions
void IRAM_ATTR timerIsr();
void taskReadCAN(void *pvParameters);

// Variable globale : nécessaire pour partager le sémaphore entre l'ISR et la tâche
static SemaphoreHandle_t semaphoreTimer = nullptr;

// SETUP *****************************************************************
void setup() {
    M5.begin();
    Serial.begin(115200);

    // Création du sémaphore binaire (créé "vide" : la tâche attendra le 1er Give)
    semaphoreTimer = xSemaphoreCreateBinary();
    if (semaphoreTimer == nullptr) {
        Serial.println("Erreur : creation du semaphore impossible");
        return;
    }

    // Création de la tâche de lecture du CAN
    xTaskCreatePinnedToCore(taskReadCAN, "readCAN", TAILLE_PILE_TACHE,
                            nullptr, PRIORITE_TACHE, nullptr, COEUR_TACHE);

    // Configuration du timer : interruption périodique toutes les 2 ms
    hw_timer_t *timer = timerBegin(NUMERO_TIMER, DIVISEUR_TIMER, true);
    timerAttachInterrupt(timer, &timerIsr, true);
    timerAlarmWrite(timer, PERIODE_TIMER_US, true);   // true = rechargement automatique
    timerAlarmEnable(timer);
}

// LOOP ******************************************************************
void loop() {
    // Tout se fait dans la tâche et l'ISR
}

/**
 * @brief ISR du timer (toutes les 2 ms) : libère uniquement le sémaphore
 *        pour réveiller la tâche taskReadCAN.
 */
void IRAM_ATTR timerIsr() {
    BaseType_t tachePlusPrioritaireReveillee = pdFALSE;

    xSemaphoreGiveFromISR(semaphoreTimer, &tachePlusPrioritaireReveillee);

    // Si la tâche réveillée est prioritaire, on demande un changement de contexte
    if (tachePlusPrioritaireReveillee == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Tâche qui, dès que le sémaphore est disponible :
 *        - lit la GPIO 36 (12 bits)
 *        - calcule la moyenne glissante des 128 derniers échantillons
 *        - envoie "échantillon<TAB>moyenne\n" sur le port série
 * @param pvParameters Non utilisé
 */
void taskReadCAN(void *pvParameters) {
    uint16_t echantillons[TAILLE_MOYENNE] = {0};  // Derniers échantillons
    uint32_t somme = 0;                           // Somme des échantillons du tableau
    uint8_t indice = 0;                           // Position du prochain échantillon

    for (;;) {
        // Attente bloquante : la tâche dort tant que l'ISR n'a pas fait Give
        if (xSemaphoreTake(semaphoreTimer, portMAX_DELAY) == pdTRUE) {
            uint16_t echantillon = analogRead(PIN_POTENTIOMETRE);

            // Moyenne glissante : on retire le plus ancien, on ajoute le nouveau 
            somme = somme - echantillons[indice] + echantillon;
            echantillons[indice] = echantillon;
            indice = (indice + 1) % TAILLE_MOYENNE;

            uint16_t moyenne = somme / TAILLE_MOYENNE;

            Serial.printf("%u\t%u\n", echantillon, moyenne);
        }
    }
}