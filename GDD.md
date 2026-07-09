# PIXELGROW — Documento de Diseño de Juego (GDD)
### Magnate botánico urbano · estética pixel 8-bit · benchmark: *Hempire*

> **Rol del documento:** propuesta de Diseñador de Economía y Sistemas. Toma *Hempire* como
> referencia estructural (no como clon) y reescribe sus pilares con un giro de ambientación,
> sistemas de cultivo activos, una genética diploide real y un bucle económico que sustituye
> el "Net Worth" por un **Índice de Regeneración Urbana**.

---

## 0. Pitch en una línea

> *Heredas un distrito de invernaderos en ruinas y reconstruyes una ciudad post-industrial
> convirtiéndola en la capital verde del país — cultivando, cruzando genética y decidiendo,
> ordenanza a ordenanza, entre el mercado regulado y el negro.*

**Fantasía de poder:** no eres un narco; eres el/la fundador/a de una cooperativa botánica que
transforma un cinturón oxidado en un ecosistema solarpunk. El dinero es un medio; el fin es la
**regeneración** (y el control blando de la política local).

---

## 1. Ambientación y giro narrativo

### 1.1 El giro respecto a *Hempire*
*Hempire* vive de la **estética criminal** (callejón, soborno, "hierba sobre ruedas"). PixelGrow
la sustituye por un **solarpunk de rust-belt**: la misma progresión "ruina → imperio", pero el
conflicto no es la policía, es la **especulación inmobiliaria y el lobby petroquímico** que quieren
demoler el barrio. El jugador pelea con biología, cooperativas y votos, no con pistolas.

- **Lugar:** *Puerto Verde*, ciudad portuaria decadente. Cinco distritos: **Los Astilleros**
  (tutorial, invernadero heredado), **El Mercado Viejo**, **La Colina** (residencial/político),
  **Zona Franca** (industria/legal) y **Las Marismas** (biotech/endgame).
- **Rationale de diseño:** un antagonista *sistémico* (especulación, contaminación, corrupción
  legal) da mejor terreno para mecánicas económicas y políticas que un antagonista *policial*, que
  solo produce mecánicas de "esconder/escapar". Además esquiva el techo de edad y de tiendas de
  la estética narco.

### 1.2 Arco en 3 actos (mapeado a progresión de sistemas)
| Acto | Fantasía | Sistema que se desbloquea |
|---|---|---|
| **I — El Cobertizo** | Sobrevivir con 2 macetas | Cultivo básico, venta directa, 1ª hibridación |
| **II — La Cooperativa** | Legalizarse, contratar, construir | Contratos, procesamiento, mercado dual, política |
| **III — El Corredor Verde** | Ciudad regenerada, biotech, liga | Genética avanzada, edificios cívicos, metajuego |

---

## 2. Elenco de personajes (con mecánica de desbloqueo)

Cada personaje **es** un sistema con cara. Diseño deliberado: el jugador no "abre un menú de
extracciones", **le pide a Rafa que monte el laboratorio**. Humaniza los tutoriales y da ganchos
narrativos a cada feature.

| Personaje | Arquetipo | Mecánica que gobierna | Cómo se desbloquea |
|---|---|---|---|
| **Tía Remedios** | Mentora / herbolaria | Tutorial de cultivo, riego, primera cepa *landrace* | Inicio |
| **Bruno "Mala Semilla"** | Genetista punk | Laboratorio de hibridación, códex de fenotipos | 1ª cosecha con calidad ≥ 60 |
| **Ing. Mei** | Ingeniera de infraestructura | Construcción de salas, clima/HVAC, expansión de plots | Nivel 4 o comprar 2ª sala |
| **Concejala Ada** | Política reformista | Consejo Municipal, ordenanzas, licencias legales | Reputación cívica ≥ 20 |
| **"El Contador"** | Intermediario gris | Mercado negro, contactos, gestión del **Calor** | Primera venta rechazada por licencia |
| **Nour** | Chef / maestra de comestibles | Cadena de valor: cocina (comestibles, tinturas) | Construir la Cocina Comunitaria |
| **Rafa** | Químico de extracción | Resinas, aceites, hachís (mayor margen, más riesgo) | Nivel 8 + licencia de laboratorio |
| **DJ Kilo** | Logística callejera | "Ruta Verde": despacho de contratos con vehículos | Acto II |
| **La Jueza Osei** | Meta/eventos | Liga cooperativa asíncrona ("Copa Puerto Verde") | Acto III |

**Rationale:** desbloqueos atados a *hitos de comportamiento* (calidad, reputación, construcción)
en vez de solo a nivel/dinero. Esto enseña sistemas en el orden correcto y evita el muro de pago
clásico de los *tycoon* móviles.

---

## 3. Núcleo de juego (Core Loop rediseñado)

### 3.1 Problema del original
El cultivo de *Hempire* es esencialmente **temporizadores + tocar para regar**. Accesible, pero
pasivo: el "skill" es abrir la app a tiempo. Queremos **actividad opcional con recompensa**, sin
castigar al jugador idle.

### 3.2 Solución: "Momentos de Intervención"
El ciclo `semilla → plántula → vegetativo → floración → cosecha` corre en tiempo real (idle-friendly),
pero cada etapa abre **ventanas** donde una micro-interacción sube calidad/rendimiento:

- **Espectro & PPM (dial minijuego):** ajusta un dial de espectro lumínico y PPM de nutrientes al
  rango objetivo de *esa cepa* (depende de su genética). Acertar el rango da bono de calidad; pasarse
  quema la planta. → convierte "más lámparas = mejor" en una decisión.
- **VPD (temperatura × humedad):** un objetivo móvil que combina dos deslizadores; mal VPD abre la
  puerta a **moho** (ver §6 plagas).
- **Defoliación (tap):** en vegetativo tardío, tocar hojas viejas concretas redirige energía a
  cogollos (rendimiento +), pero quitar demasiadas estresa (calidad −).
- **QTE de plaga:** si aparece una plaga, un mini-evento de reacción rápida (o gastar item) la trata.

**Regla de oro (accesibilidad):** *ignorar* todas las intervenciones nunca hace **perder** la
cosecha; solo produce calidad "base". El jugador activo obtiene el techo; el idle obtiene el suelo.
Esto respeta el público móvil y premia la maestría.

### 3.3 Maestría por cepa
Cosechar repetidamente una misma genética sube su **Maestría** (0–5★). Cada estrella: +rendimiento,
+velocidad, y desbloquea su **fenotipo estable** para hibridar sin varianza. Da razón para
re-cultivar y no solo "coleccionar y saltar".

---

## 4. Hibridación / Genética (el corazón diferenciador)

### 4.1 De "promediar stats" a genética **diploide**
*Hempire* cruza dos cepas y promedia potencia/velocidad/rendimiento. Nosotros modelamos un
**genoma diploide** con herencia mendeliana simplificada:

- Cada rasgo está codificado por **genes** con **dos alelos** (uno de cada progenitor).
- Los alelos tienen **dominancia** (dominante `A` / recesivo `a`). El **genotipo** (par de alelos)
  determina el **fenotipo** expresado.
- Alelos **recesivos raros** guardan los rasgos legendarios: solo se expresan en homocigosis
  (`aa`), por eso las cepas élite requieren *retrocruzar* y estabilizar linajes — un metajuego de
  breeding con profundidad real, no azar plano.

### 4.2 Rasgos modelados
| Gen | Fenotipo | Efecto de juego |
|---|---|---|
| `potencia` | THC/CBD ratio | precio, demanda de nicho (medicinal vs recreativo) |
| `rendimiento` | gramos/cosecha | economía directa |
| `velocidad` | días de ciclo | throughput |
| `resistencia` | defensa a plaga/moho | menos micro-gestión |
| `terpenos` | perfil aromático (cítrico, terroso, floral…) | **precio premium + catas de liga** |
| `morfologia` | indica/sativa/híbrido (altura, hoja) | **visual** + interacción con clima |
| `color` | pigmento (verde→púrpura→dorado) | **visual** + valor cosmético |

### 4.3 Epigenética (el guiño maestro)
Rasgos **latentes** que solo se **expresan según el ambiente de cultivo**:
- Estrés por frío controlado en floración → **expresión púrpura** (antocianinas) → +valor cosmético.
- Fotoperiodo/luz UV alta → +tricomas → +potencia visible (más "escarcha" en el sprite).

**Rationale:** el genotipo es el potencial; el *cómo cultivas* decide el fenotipo final. Dos
jugadores con la misma semilla obtienen plantas distintas. Esto es lo que *Hempire* no tiene y lo
que da rejugabilidad y "presunción de coleccionista".

### 4.4 Genética → Visual (pixel)
El sprite de la planta se **compone por genes**, no es fijo:
- `morfologia` elige el esqueleto de hoja (indica = ancha/compacta, sativa = fina/alta).
- `color` desplaza la paleta (HSL) del follaje y cogollo.
- `potencia`/epigenética añade capas de **tricomas** (partículas brillantes) sobre los cogollos.
- Rareza añade un **aura**. → cada cepa es literalmente reconocible de un vistazo.

---

## 5. Cadena de valor (procesamiento)

Materia prima (cogollo) → productos con mejor margen y menor perecibilidad. Cada rama tiene su
personaje y su edificio.

| Rama | Personaje | Entrada → Salida | Diseño |
|---|---|---|---|
| **Curado** | Tía Remedios | cogollo fresco → cogollo curado | +calidad con el tiempo (frascos, §7 perecibilidad) |
| **Cocina** | Nour | cogollo + insumos → comestibles/tinturas | producto estable, demanda distinta (medicinal) |
| **Extracción** | Rafa | cogollo → resina/aceite/hachís | mayor margen, requiere licencia, riesgo de merma |
| **Textil/Hemp** | Ing. Mei | biomasa → fibra/bioplástico | rama "cívica": alimenta construcción y baja contaminación |

La rama **Textil/Hemp** es nueva vs *Hempire* y conecta cultivo ↔ regeneración urbana: convierte
descarte en materiales de construcción, cerrando el bucle ecológico (y el económico).

---

## 6. Plagas, clima y salud (presión sobre el sistema)

- **Clima diario** (soleado/nublado/húmedo/ola de calor/frío) modifica velocidad, consumo de agua y
  **riesgo de moho**. Introduce planificación: no plantar tu joya justo antes de una semana húmeda.
- **Plagas** (araña roja, moho, mosca): probabilidad por tick modulada por clima y `resistencia`
  genética. Sin tratar → drena salud, calidad y rendimiento. Se tratan con item o QTE.
- **Rationale:** la presión da sentido al gen `resistencia`, a las intervenciones activas y al gasto
  del growshop. Sin presión, no hay decisiones económicas.

---

## 7. Economía y progresión urbana

### 7.1 Sustituir "Net Worth" por **Índice de Regeneración del Barrio (IRB)**
En *Hempire* compras inmuebles para inflar un número (`Net Worth`) que desbloquea zonas. Es plano.
Aquí, reinvertir construye **edificios cívicos** que dan **bonos sistémicos**, no solo un número:

| Edificio | Coste (materiales+$) | Bono pasivo | Desbloquea |
|---|---|---|---|
| Clínica comunitaria | — | +demanda producto medicinal | reputación cívica |
| Cooperativa | — | −impuestos, +precio regulado | contratos legales |
| Parque / fitorremediación | biomasa hemp | −contaminación, +IRB | zonas nuevas |
| Laboratorio | $$$ | rama de extracción | Rafa |
| Mercado techado | — | +estabilidad de precios | menos volatilidad |

**Bucle nuevo:** `Producir → Vender/Contratar → Materiales+$ → Construir cívico → ↑IRB + bonos →
↑Influencia política → desbloquear zona/ordenanza → producir mejor`. El IRB es a la vez *score* y
*multiplicador*, así que reinvertir se siente sistémico, no cosmético.

### 7.2 Impacto político (nuevo eje)
Un **Consejo Municipal** con una barra de **Influencia**. Gastando influencia (ganada por IRB,
donaciones, reputación) el jugador **aprueba ordenanzas**:
- *Legalización progresiva* → convierte demanda del mercado negro en regulado.
- *Licencias* → habilita extracción/venta legal de más productos.
- *Subvención verde* → ingreso pasivo por IRB.
- *Zonificación* → desbloquea distrito.

Trade-off: las ordenanzas pro-regulación **suben impuestos** pero **bajan el Calor policial**. El
jugador elige su doctrina económica. Esto es el "soborno al asistente del alcalde" de *Hempire*,
reconvertido en un **árbol de políticas** con decisiones reales.

### 7.3 Mercado dual: regulado ⇄ negro
| | Mercado **Regulado** | Mercado **Negro** |
|---|---|---|
| Margen | Menor (impuestos) | Mayor |
| Estabilidad | Alta (precio suelo) | Volátil (demanda diaria, moods) |
| Requisitos | Licencia, trazabilidad, calidad mín. | Ninguno |
| Riesgo | Ninguno | Sube **Calor** → probabilidad de **redada** (pierdes stock) |
| Reputación | +cívica (desbloquea política) | +callejera (desbloquea contactos/precios) |

El **Calor** decae con el tiempo y con ordenanzas; el jugador gestiona un termostato de riesgo. La
tensión regulado/negro es el minijuego económico central del midgame.

---

## 8. Logística: contratos ("Ruta Verde")

Rediseño del *Weed on Wheels*. Un **tablero de despacho** con:
- **Contratos** = pedido `{tipo/terpeno/potencia mín, calidad mín, cantidad, ventana horaria, recompensa}`.
- **Recompensa dual:** los contratos **regulados** pagan **materiales de construcción + licencias**;
  los **negros** pagan **$ + reputación callejera** pero suman Calor.
- **Perecibilidad:** el producto tiene **frescura** que decae; contratos con ventana premian tener
  stock curado/procesado listo (la cadena de valor §5 alimenta esto).
- **Flota:** vehículos con **capacidad** y **velocidad**; asignas rutas (optimización ligera:
  llenar el camión, cubrir ventanas). Mejorable en el growshop.

**Rationale:** convierte "entregar por materiales raros" en un **puzzle de asignación** ligero, con
decisiones (¿cumplo el contrato legal lento o el negro rápido y arriesgado?), en vez de un simple
temporizador.

---

## 9. Metajuego competitivo — "Copa Puerto Verde"

Liga **asíncrona** semanal (evolución de la Copa Hempire): envías tu mejor cepa a una **cata** que
puntúa **potencia + calidad + perfil de terpenos + rareza de fenotipo**. Novedad:
- **Ligas por doctrina:** categoría "medicinal" (premia CBD/terpenos) vs "recreativa" (premia THC),
  para que builds distintos tengan ligas distintas (evita el meta único).
- **Cooperativo de barrio:** tu puntuación suma al ranking colectivo "ciudad más verde" → recompensas
  compartidas. Fomenta comunidad, no solo whales.

---

## 10. Arquitectura de datos inicial (TypeScript)

> Interfaces limpias que modelan la visión. Diseñadas para serializar a JSON (save idle) y para que
> el motor de render derive el sprite del genoma.

### 10.1 Genética y Planta

```typescript
type Distrito = "astilleros" | "mercado_viejo" | "la_colina" | "zona_franca" | "marismas";
type Morfologia = "indica" | "sativa" | "hibrido";
type EtapaCultivo = "semilla" | "plantula" | "vegetativo" | "floracion" | "cosecha";
type Terpeno = "citrico" | "terroso" | "floral" | "diesel" | "dulce" | "pino";
type Rareza = "comun" | "rara" | "epica" | "legendaria" | "mitica";

/** Un alelo: valor 0..1 + si es dominante y si es un alelo raro/legendario. */
interface Alelo {
  valor: number;        // expresión potencial del rasgo (0..1)
  dominante: boolean;   // domina sobre el recesivo al expresar el fenotipo
  raro?: boolean;       // alelo legendario: solo brilla en homocigosis (aa)
}

/** Genoma diploide: cada rasgo tiene 2 alelos (uno por progenitor). */
interface Genoma {
  potencia:    [Alelo, Alelo];
  rendimiento: [Alelo, Alelo];
  velocidad:   [Alelo, Alelo];
  resistencia: [Alelo, Alelo];
  terpenos:    [Alelo, Alelo];   // el valor mapea a un Terpeno dominante
  morfologia:  [Alelo, Alelo];
  color:       [Alelo, Alelo];   // pigmento base 0..1 (HSL shift)
  /** Rasgos LATENTES que solo se expresan según el ambiente (epigenética). */
  latentes: {
    purpura: boolean;   // se expresa con estrés por frío en floración
    escarcha: boolean;  // se expresa con luz UV/fotoperiodo alto
  };
}

/** Fenotipo = lo que realmente se expresó (genotipo + ambiente). Es lo que se ve y se vende. */
interface Fenotipo {
  potencia: number; rendimiento: number; velocidad: number; resistencia: number;
  terpenoDominante: Terpeno;
  morfologia: Morfologia;
  colorHue: number;          // 0..360, derivado de color + latentes.purpura
  escarchaExpresada: boolean;
  rareza: Rareza;            // derivada del promedio + presencia de alelos raros
}

interface Cepa {
  id: string;
  nombre: string;
  genoma: Genoma;
  fenotipoEstable?: Fenotipo; // fijado al llegar a 5★ de maestría
  generacion: number;         // nº de cruces en el linaje
  maestria: number;           // 0..5 estrellas
  semillas: number;
  linaje: [string, string] | null; // ids de progenitores
}

/** Instancia viva en una maceta (deriva su render del genoma de la Cepa). */
interface Planta {
  cepaId: string;
  etapa: EtapaCultivo;
  progreso: number;           // 0..1 dentro de la etapa
  // recursos y salud
  agua: number;               // 0..100
  nutrientes: number;         // 0..100
  ppm: number;                // objetivo depende de la cepa (minijuego)
  vpd: number;                // temp×humedad balance 0..1
  salud: number;              // 0..100
  calidadAcumulada: number;   // 0..100, techo según intervenciones activas
  // ambiente/epigenética aplicada durante ESTE ciclo
  ambiente: { frioFloracion: boolean; uvAlta: boolean };
  // presión
  plaga: { tipo: "arana" | "moho" | "mosca"; severidad: number } | null;
  plantadaEnDia: number;
}
```

### 10.2 Contrato de entrega (logística dual)

```typescript
type Canal = "regulado" | "negro";

interface RequisitoProducto {
  tipoProducto: "cogollo" | "comestible" | "extracto" | "fibra";
  morfologia?: Morfologia;      // opcional: exige indica/sativa/híbrido
  terpeno?: Terpeno;            // opcional: exige perfil aromático
  potenciaMin: number;         // 0..1
  calidadMin: number;          // 0..100
  cantidad: number;            // gramos/unidades
}

interface RecompensaContrato {
  dinero: number;
  materiales?: number;          // solo canal regulado → construcción cívica
  licencia?: string;            // solo regulado → desbloquea rama/producto
  reputacionCivica?: number;    // regulado
  reputacionCalle?: number;     // negro
  calor?: number;               // negro: sube el termómetro de riesgo
}

interface ContratoDeEntrega {
  id: string;
  cliente: string;              // nombre/arquetipo
  canal: Canal;
  requisito: RequisitoProducto;
  recompensa: RecompensaContrato;
  // ventana logística
  diaEmision: number;
  diaLimite: number;            // expira → penalización de reputación
  frescuraMinima: number;       // 0..1: exige producto no perecido
  // asignación de flota
  vehiculoAsignado?: string | null;
  estado: "abierto" | "en_ruta" | "cumplido" | "expirado" | "fallido";
}
```

### 10.3 Estado del barrio / ciudad (regeneración + política)

```typescript
interface EdificioCivico {
  id: string;
  tipo: "clinica" | "cooperativa" | "parque" | "laboratorio" | "mercado";
  nivel: number;
  costeMateriales: number;
  bono: Partial<Record<"demandaMedicinal" | "impuestos" | "contaminacion"
        | "estabilidadPrecio" | "ingresoPasivo", number>>;
}

interface Ordenanza {
  id: string;
  nombre: string;
  costeInfluencia: number;
  aprobada: boolean;
  efecto: {
    convierteDemandaNegroAregulado?: number; // 0..1
    modImpuestos?: number;                    // +/-
    modCalorPasivo?: number;                  // decae el Calor
    desbloqueaDistrito?: Distrito;
    licenciaOtorgada?: string;
  };
}

interface EstadoBarrio {
  distrito: Distrito;
  irb: number;                  // Índice de Regeneración del Barrio (score+multiplicador)
  contaminacion: number;        // 0..100, baja con parques/hemp
  edificios: EdificioCivico[];
  desbloqueado: boolean;
}

interface EstadoCiudad {
  dia: number;
  efectivo: number;
  materiales: number;
  // reputaciones separadas: cada una abre puertas distintas
  reputacionCivica: number;
  reputacionCalle: number;
  // termómetro de riesgo del mercado negro
  calor: number;                // 0..100 → probabilidad de redada
  // política
  influencia: number;
  ordenanzas: Ordenanza[];
  // mundo
  barrios: Record<Distrito, EstadoBarrio>;
  climaHoy: { id: string; velocidad: number; moho: number; agua: number };
  // mercado dinámico
  mercado: {
    moodRegulado: number;
    demandaNegro: Record<Morfologia, number>;
  };
}
```

### 10.4 Ejemplo JSON (semilla legendaria retrocruzada)

```json
{
  "id": "cepa_galaxy_og",
  "nombre": "Galaxy OG",
  "generacion": 6,
  "maestria": 5,
  "semillas": 3,
  "linaje": ["cepa_purple_kush", "cepa_northern_x"],
  "genoma": {
    "potencia":    [{"valor": 0.94, "dominante": true, "raro": true},
                    {"valor": 0.91, "dominante": true, "raro": true}],
    "rendimiento": [{"valor": 0.80, "dominante": true},  {"valor": 0.72, "dominante": false}],
    "velocidad":   [{"valor": 0.70, "dominante": false}, {"valor": 0.66, "dominante": true}],
    "resistencia": [{"valor": 0.85, "dominante": true},  {"valor": 0.83, "dominante": true}],
    "terpenos":    [{"valor": 0.62, "dominante": true},  {"valor": 0.40, "dominante": false}],
    "morfologia":  [{"valor": 0.15, "dominante": true},  {"valor": 0.20, "dominante": true}],
    "color":       [{"valor": 0.72, "dominante": true},  {"valor": 0.68, "dominante": false}],
    "latentes": { "purpura": true, "escarcha": true }
  }
}
```

**Nota de arquitectura:** el `Genoma` es la *fuente de verdad* persistida; el `Fenotipo` es
**derivado** (genotipo + `ambiente`) y se recalcula al render/cosecha. Esto mantiene el save
compacto y hace que la epigenética "simplemente funcione" sin duplicar estado.

---

## 11. Roadmap de implementación sobre `pixelgrow.html`

Prioridad de features para acercar el prototipo actual a este GDD:

1. **v2 (esta iteración):** plagas + clima + curado · cepas élite (códex) · contratos/clientes ·
   sprites dibujados a mano · chiptune + partículas de tricomas.
2. **v3:** genoma diploide real (sustituir promedios), epigenética visual (púrpura/escarcha),
   maestría por cepa.
3. **v4:** mercado dual regulado/negro + Calor · Consejo Municipal + ordenanzas · edificios cívicos
   e IRB.
4. **v5:** cadena de valor (cocina/extracción/hemp) · flota y "Ruta Verde" · Copa Puerto Verde.

---

*PixelGrow GDD v1 — documento vivo. Ficción de simulación botánica; sin contenido operativo real.*
