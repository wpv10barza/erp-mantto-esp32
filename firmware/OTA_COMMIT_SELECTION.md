# Seleccionar firmware por commit (sin ejecutar git pull en ESP32)

Pantalla: **Actualizar firmware → BUSCAR OTA / COMMIT / INSTALAR**.

- **BUSCAR OTA** restablece la selección y consulta `/api/device/v1/firmware/latest`.
- **COMMIT** abre el editor táctil; introduzca SHA de 8-40 caracteres (el
  teclado ofrece ABC y 123). ENTER consulta
  `/api/device/v1/firmware/commits/{sha}`. Nunca envía una orden 3C.
- **INSTALAR** vuelve a consultar el mismo commit y solo escribe flash
  si Databricks confirma una versión estrictamente superior y devuelve
  binario compatible, tamaño y SHA-256 verificables.
- Si no se encuentra el commit o hay varios con el mismo prefijo,
  no hay fallback a latest ni flasheo; el usuario puede ingresar el SHA
  completo. El instalador solo acepta URL relativa OTA del mismo backend.

El commit `a4b4144c6d96aedf8ca6a78f9b7adf7a436c0b0e` existe en GitHub,
pero una ejecución de compilación original falló, así que no equivale a
una versión instalable. Debe haber una release válida registrada con el
campo `git_commit_sha` exacto en Unity Catalog y el binario apropiado.
Nunca descargar ZIP de source ni compilar código GitHub sobre el ESP32.

El instalador se prepara en `feature/databricks-ota-manual`, backend en
`feature/ota-content-verification`. Primero se exige CI, primer flash
USB con particiones OTA, tokens vigentes, publicación de release real y
prueba física. El estado del backend desplegado se verifica aparte.
