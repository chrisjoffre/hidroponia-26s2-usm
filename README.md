# hidroponia-26s2-usm


# Sistema de cultivo hidropónico automatizado

Proyecto inicial de ingenieria civil electrónica utfsm, (PENDIENTE: su propósito y las funcionalidades principales).
Fecha de inicio: Noviembre 2026

## 👥 Equipo de desarrollo

- Gabriel Hevia
- Christian Jofré
- Vicente Muñoz
- Vicente Valenzuela


## 📁 Estructura del repositorio

```text
proyecto-hidroponia/
│
├── src/                         # Código fuente para Arduino UNO R4
│   ├── main/                    # Código principal del prototipo
│   │   └── main.ino
│   │
│   ├── tests/                   # Pruebas individuales por sensor/actuador
│   │   ├── test_ph/
│   │   ├── test_ec/
│   │   ├── test_temp_agua/
│   │   └── test_bomba/
│   │
│   └── lib/                     # Librerías externas o módulos propios
│
├── hardware/                    # Esquemáticos, simulaciones y diseños físicos
│   ├── schematics/              # Diagramas de conexión (Fritzing, KiCad, PDF/PNG)
│   ├── simulations/             # Circuitos LTspice (.asc)
│   └── cad_3d/                  # Diseños STL/CAD de soportes y estructura
│
├── docs/                        # Entregables y material metodológico (IWG400)
│   ├── bitacora/                # Entrevistas, Herramientas 7, 12, 13, etc.
│   ├── latex/                   # Fuentes .tex e imágenes del informe final
│   └── datasheets/              # Hojas de datos de sensores y Arduino UNO R4
│
├── media/                       # Recursos visuales
│   └── img/                     # Diagramas, fotos y storyboard
│
├── .gitignore
├── LICENSE
└── README.md                    # Presentación, equipo, componentes y guía de ejecución
```

