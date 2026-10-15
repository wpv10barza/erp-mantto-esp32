# ERP Mantto ESP32 — LangSmith Deployment

Este repositorio incluye una capa LangGraph desplegable en LangSmith.

## Configuración

- Rama preparada: `langsmith-deploy`
- Archivo de configuración: `langgraph.json`
- Graph ID: `erp_mantto_esp32`
- Python: 3.11

## Estado del flujo

El grafo reproduce el contrato de control del proyecto:

1. `action=propose` → `pending_confirmation`
2. `action=confirm` → `applied`
3. `action=reject` → `rejected`

No ejecuta cambios de mantenimiento automáticamente. La confirmación o rechazo debe ser una acción explícita del operador.

## Ejemplo de entrada

```json
{
  "device_id": "esp32-panel",
  "command": "Preparar propuesta de mantenimiento para bomba P-101",
  "action": "propose"
}
```

También admite entrada mediante `messages` para pruebas en Studio.

## Despliegue en LangSmith

En LangSmith, abra **Deployments → New Deployment → Import from GitHub** y seleccione:

- Repository: `wpv10barza/erp-mantto-esp32`
- Branch: `langsmith-deploy` (o `main` después de fusionar)
- Config file: `langgraph.json`
- Deployment type: Development para la primera validación

No se requieren secretos de modelo para esta primera versión, porque el grafo es determinístico. Si posteriormente se incorpora un LLM o un servicio externo, configure sus claves y URLs como secretos/variables del deployment, no dentro del repositorio.
