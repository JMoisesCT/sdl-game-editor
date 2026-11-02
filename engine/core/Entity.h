#pragma once
#include <cstdint>

// Una entidad es SOLO un numero: no tiene datos ni comportamiento. Lo que la hace
// "jugador" o "fruta" son los componentes que el World le asocia a ese numero.
//
// Los ids NUNCA se reutilizan dentro de un World (cuentan hacia arriba desde 1), asi
// que guardar un Entity y preguntar mas tarde world.isAlive(e) es seguro: si la
// entidad se destruyo, la respuesta es false y no hay puntero colgante.
// El id tambien se guarda en el archivo de escena, asi que es estable entre sesiones.

using Entity = std::uint32_t;

constexpr Entity NullEntity = 0; // "ninguna entidad"
