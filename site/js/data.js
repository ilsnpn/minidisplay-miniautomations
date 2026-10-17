// Données de la carte pour l'explorateur 3D (sources : wiki Waveshare, datasheet ESP32-C6).
// Script classique (pas un module) pour que la page s'ouvre aussi en double-clic (file://).

window.BOARD_DATA = {
  // Rangée A (bord « BOOT ») puis rangée B (bord « RST »), de gauche à droite, face composants, USB à droite.
  rowA: ['5', '4', '3', '2', '1', '0', '3V3', 'GND', '5V'],
  rowB: ['9', '18', '19', '20', '23', '12', '13', 'RXD', 'TXD'],

  pins: {
    '5V': {
      type: 'power', title: '5V', sub: 'Alimentation',
      fn: ['VBUS USB', 'Entrée / sortie 5 V'],
      desc: "Relié au 5 V de l'USB-C. Il peut alimenter la carte sans USB, ou fournir le 5 V de l'USB à un module externe.",
      warn: "Pas d'alimentation externe sur cette broche pendant que l'USB-C est branché.",
      project: "Inutilisée : la carte est alimentée par la docking via l'USB-C."
    },
    'GND': {
      type: 'gnd', title: 'GND', sub: 'Masse',
      fn: ['0 V'],
      desc: 'Masse commune de la carte. Tout module externe doit partager cette masse.',
      project: 'Inutilisée.'
    },
    '3V3': {
      type: 'power', title: '3V3', sub: 'Sortie 3,3 V',
      fn: ['Sortie LDO ME6217', '800 mA max'],
      desc: "Sortie du régulateur ME6217C33. Ses 800 mA sont partagés avec l'ESP32-C6, l'écran et la carte TF : prévoir de la marge pour les pics du Wi-Fi.",
      warn: "C'est une sortie : ne jamais y injecter de tension.",
      project: 'Inutilisée.'
    },
    '0': {
      type: 'gpio', title: 'GPIO0', sub: 'GPIO basse consommation',
      fn: ['ADC1_CH0', 'XTAL_32K_P', 'LP GPIO', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: "Entrée analogique, ou patte du quartz 32 kHz optionnel. Comme toutes les broches GPIO0 à 7, elle reste pilotable par le cœur LP pendant le sommeil profond.",
      project: 'Libre.'
    },
    '1': {
      type: 'gpio', title: 'GPIO1', sub: 'GPIO basse consommation',
      fn: ['ADC1_CH1', 'XTAL_32K_N', 'LP GPIO', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: "Entrée analogique, ou seconde patte du quartz 32 kHz optionnel. GPIO basse consommation.",
      project: 'Libre.'
    },
    '2': {
      type: 'gpio', title: 'GPIO2', sub: 'GPIO basse consommation',
      fn: ['ADC1_CH2', 'LP GPIO', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: 'Entrée analogique ou GPIO polyvalent. Un bon choix pour un capteur analogique.',
      project: 'Libre.'
    },
    '3': {
      type: 'gpio', title: 'GPIO3', sub: 'GPIO basse consommation',
      fn: ['ADC1_CH3', 'LP GPIO', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: 'Entrée analogique ou GPIO polyvalent.',
      project: 'Libre.'
    },
    '4': {
      type: 'shared', title: 'GPIO4', sub: 'Partagée avec la carte TF',
      fn: ['ADC1_CH4', 'SD_CS', 'MTMS (JTAG)', 'Strapping'],
      desc: 'Sélection (CS) de la carte TF sur le bus SPI commun avec l\'écran. Elle est aussi lue au démarrage (broche de strapping).',
      warn: 'Inutilisable comme GPIO si la carte TF est utilisée.',
      project: 'Réservée à la carte TF (non utilisée pour l\'instant).'
    },
    '5': {
      type: 'shared', title: 'GPIO5', sub: 'Partagée avec la carte TF',
      fn: ['ADC1_CH5', 'SD_MISO', 'MTDI (JTAG)', 'Strapping'],
      desc: 'Ligne MISO du bus SPI : la carte TF y renvoie ses données. Elle est aussi lue au démarrage (broche de strapping).',
      warn: 'Inutilisable comme GPIO si la carte TF est utilisée.',
      project: 'Déclarée comme MISO du bus SPI.'
    },
    '9': {
      type: 'shared', title: 'GPIO9', sub: 'Bouton BOOT',
      fn: ['BOOT', 'Strapping', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: "Reliée au bouton BOOT, active à l'état bas. Maintenue basse au reset, elle démarre la puce en mode téléchargement. Une fois le programme lancé, elle sert de bouton utilisateur.",
      warn: 'Ne rien y brancher qui la tire vers le bas au démarrage.',
      project: "Lue par le firmware : appui court pour changer de vue, appui long pour la luminosité."
    },
    '18': {
      type: 'gpio', title: 'GPIO18', sub: 'GPIO',
      fn: ['PWM', 'I2C', 'I2S', 'UART'],
      desc: 'GPIO polyvalent, sans ADC. Toutes les fonctions numériques passent par la matrice GPIO.',
      project: 'Libre.'
    },
    '19': {
      type: 'gpio', title: 'GPIO19', sub: 'GPIO',
      fn: ['PWM', 'I2C', 'I2S', 'UART'],
      desc: 'GPIO polyvalent, sans ADC.',
      project: 'Libre.'
    },
    '20': {
      type: 'gpio', title: 'GPIO20', sub: 'GPIO',
      fn: ['PWM', 'I2C', 'I2S', 'UART'],
      desc: 'GPIO polyvalent, sans ADC.',
      project: 'Libre.'
    },
    '23': {
      type: 'gpio', title: 'GPIO23', sub: 'GPIO',
      fn: ['PWM', 'I2C', 'I2S', 'UART'],
      desc: 'GPIO polyvalent, sans ADC.',
      project: 'Libre.'
    },
    '12': {
      type: 'danger', title: 'GPIO12', sub: 'USB D−',
      fn: ['USB D−', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: "Ligne D− de l'USB natif de l'ESP32-C6, reliée au connecteur USB-C.",
      warn: "S'en servir comme GPIO coupe l'USB : plus de téléversement ni de console, et le pont MDMA perd la carte.",
      project: "Indispensable : c'est par là qu'arrivent les données du quota."
    },
    '13': {
      type: 'danger', title: 'GPIO13', sub: 'USB D+',
      fn: ['USB D+', 'PWM', 'I2C', 'I2S', 'UART'],
      desc: "Ligne D+ de l'USB natif de l'ESP32-C6, reliée au connecteur USB-C.",
      warn: "S'en servir comme GPIO coupe l'USB : plus de téléversement ni de console, et le pont MDMA perd la carte.",
      project: "Indispensable : c'est par là qu'arrivent les données du quota."
    },
    'RXD': {
      type: 'uart', title: 'RXD · GPIO17', sub: 'UART0 RX',
      fn: ['U0RXD', 'GPIO17'],
      desc: 'Réception de l\'UART0, le port série « historique » du bootloader ROM. Pratique pour brancher un module série externe.',
      project: 'Inutilisée : la console passe par l\'USB natif (CDC).'
    },
    'TXD': {
      type: 'uart', title: 'TXD · GPIO16', sub: 'UART0 TX',
      fn: ['U0TXD', 'GPIO16'],
      desc: 'Émission de l\'UART0. Le bootloader ROM y écrit ses messages de démarrage à 115200 bauds.',
      project: 'Inutilisée : la console passe par l\'USB natif (CDC).'
    }
  },

  components: {
    esp32: {
      title: 'ESP32-C6FH4', sub: 'Microcontrôleur',
      fn: ['RISC-V 160 MHz', 'Cœur LP 20 MHz', '512 Ko SRAM', '4 Mo Flash', 'Wi-Fi 6', 'BLE 5', 'Zigbee / Thread', 'QFN 5 × 5 mm'],
      desc: "Le cerveau de la carte. Un cœur RISC-V principal à 160 MHz, un second cœur basse consommation, et une Flash de 4 Mo intégrée au boîtier. Il n'y a pas de PSRAM, donc l'image de l'écran (110 Ko) tient de justesse en SRAM.",
      project: "Il fait tourner le firmware claude_usage : il lit les trames USB et dessine l'écran hors champ avant de l'envoyer d'un bloc."
    },
    ldo: {
      title: 'ME6217C33M5G', sub: 'Régulateur 3,3 V',
      fn: ['LDO', '800 mA max', 'SOT-23-5'],
      desc: "Il abaisse le 5 V de l'USB en 3,3 V pour toute la carte. La broche 3V3 est sa sortie. Absent du modèle CAO : sa zone cliquable reprend sa position sur la photo Waveshare.",
      project: 'Alimente tout le montage.'
    },
    tf: {
      title: 'Emplacement carte TF', sub: 'microSD en SPI',
      fn: ['CS : GPIO4', 'MISO : GPIO5', 'MOSI : GPIO6', 'SCLK : GPIO7'],
      desc: "Lecteur microSD sur le même bus SPI que l'écran. Il peut servir à stocker des images, des logos ou des journaux.",
      project: 'Pas encore utilisé. Il pourrait stocker les logos des notifications.'
    },
    antenna: {
      title: 'Antenne céramique', sub: '2,4 GHz',
      fn: ['Wi-Fi', 'Bluetooth LE', '802.15.4'],
      desc: 'Antenne intégrée pour toutes les radios du C6. Ne pas la masquer avec du métal.',
      project: "Inutilisée : MDMA reste en filaire pour ne pas apparaître sur le réseau de l'entreprise."
    },
    boot: {
      title: 'Bouton BOOT', sub: 'GPIO9',
      fn: ['Mode téléchargement', 'Bouton utilisateur'],
      desc: 'Maintenir BOOT, appuyer sur RESET puis relâcher : la puce passe en mode téléchargement. En fonctionnement normal, c\'est un bouton libre.',
      project: "Appui court : vue suivante (jauges, gros chiffres, horloge). Appui long : luminosité 100 %, 45 % ou 15 %. Les deux sont mémorisés."
    },
    reset: {
      title: 'Bouton RESET', sub: 'EN',
      fn: ['Redémarrage'],
      desc: "Il redémarre l'ESP32-C6. Combiné à BOOT, il permet d'entrer en mode téléchargement.",
      project: 'Redémarre la carte. Le pont la retrouve tout seul.'
    },
    usb: {
      title: 'USB-C', sub: 'USB Serial/JTAG natif',
      fn: ['Alimentation 5 V', 'Téléversement', 'Console série', 'Débogage JTAG'],
      desc: "Relié directement à l'USB intégré de l'ESP32-C6 (GPIO12/13) : la carte n'a pas de puce USB-série. Windows la voit comme un port COM (VID 303A).",
      project: 'Le seul lien avec le PC : alimentation, firmware, et les trames de quota toutes les 30 s.'
    },
    lcd: {
      title: 'Écran 1,47″ IPS', sub: 'ST7789 · 172 × 320',
      fn: ['SPI 40 MHz', '262K couleurs', 'MOSI 6', 'SCLK 7', 'CS 14', 'DC 15', 'RST 21', 'BL 22'],
      desc: "Dalle IPS pilotée par un ST7789. Le contrôleur gère 240 colonnes pour une dalle de 172 : l'image est décalée de 34 pixels. Le rétroéclairage (GPIO22) est réglable en PWM.",
      project: "Affiche le quota en paysage (320 × 172). L'image est rendue en mémoire puis envoyée d'un bloc, sans scintillement."
    },
    led: {
      title: 'LED RGB', sub: 'GPIO8 · adressable',
      fn: ['WS2812', 'GPIO8 (strapping)'],
      desc: "LED adressable placée au centre de la carte, côté écran, sous la plaque d'acrylique transparente qu'elle éclaire.",
      project: 'Elle prend la nuance de la jauge de session : terracotta dans le rythme, vers le rouge si la consommation va trop vite, vers le gris chaud s\'il reste de la marge.'
    },
    crystal: {
      title: 'Quartz 40 MHz', sub: 'Horloge principale',
      fn: ['40 MHz'],
      desc: "Il cadence l'ESP32-C6 et ses radios.",
      project: '—'
    }
  }
};
