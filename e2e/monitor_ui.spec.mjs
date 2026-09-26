import { test, expect } from '@playwright/test';

const BASE_URL = (process.env.MONITOR_BASE_URL || 'http://127.0.0.1:3000').replace(/\/$/, '');
const DEVICE_TOKEN = process.env.ESP32_API_TOKEN || 'ci-e2e-token';

test('real Monitor UI completes ESP32 command through preview and human confirmation', async ({ page, request }) => {
  let sheetsPutSeen = false;
  let proposalCreated = false;
  let proposalApproved = false;
  let sheetsPayload = null;

  page.on('request', (requestEvent) => {
    const url = new URL(requestEvent.url());
    if (url.pathname === '/api/review/proposals' && requestEvent.method() === 'POST') proposalCreated = true;
    if (url.pathname.endsWith('/approve') && url.pathname.startsWith('/api/review/proposals/') && requestEvent.method() === 'POST') {
      proposalApproved = true;
    }
  });

  await page.route('**/api/extract', async route => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({
        tarea_buscada: 'J10',
        tarea_id: '',
        columna_busqueda: 'F',
        operaciones: [{
          campo: 'frecuencia',
          columna_actualizar: 'L',
          encabezado: 'Frecuencia',
          valor_actualizar: 1,
          razon: 'CI E2E',
        }],
        requiere_revision: false,
        motivo_revision: '',
      }),
    });
  });

  await page.route('https://sheets.googleapis.com/**', async route => {
    const requestEvent = route.request();
    const url = new URL(requestEvent.url());
    const encodedRange = url.pathname.split('/values/')[1] || '';
    const range = decodeURIComponent(encodedRange);

    if (requestEvent.method() === 'PUT') {
      sheetsPutSeen = true;
      sheetsPayload = requestEvent.postDataJSON();
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ updatedCells: 1 }),
      });
      return;
    }

    if (range.includes('Data!A4:AF4')) {
      const headers = Array.from({ length: 32 }, () => '');
      headers[4] = 'TareaId';
      headers[5] = 'Nombre';
      headers[11] = 'Frecuencia';
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ values: [headers] }),
      });
      return;
    }

    if (range.includes('Data!B5:C')) {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ values: [['Item demo', 'Modo demo']] }),
      });
      return;
    }

    if (range.includes('Data!N5:O')) {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ values: [['Mecanico', 'Labor demo']] }),
      });
      return;
    }

    if (range.includes('Data!F5:F')) {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ values: [['J10']] }),
      });
      return;
    }

    await route.fulfill({
      status: 404,
      contentType: 'application/json',
      body: JSON.stringify({ error: 'unexpected sheets range', range }),
    });
  });

  await page.goto(BASE_URL, { waitUntil: 'domcontentloaded' });
  await expect(page.getByText('Automatización 3C')).toBeVisible();

  const command = {
    device_id: 'panel-4848s040-3c-ui-e2e',
    request_id: 'panel-4848s040-3c-ui-e2e-001',
    text: 'Cambia la tarea J10 a mensual',
  };

  const enqueue = await request.post(`${BASE_URL}/api/device/v1/commands`, {
    headers: {
      'content-type': 'application/json',
      'x-3c-device-token': DEVICE_TOKEN,
    },
    data: command,
  });
  expect(enqueue.status()).toBe(202);
  const queued = await enqueue.json();
  expect(queued.status).toBe('pending_confirmation');

  await expect(page.getByText(command.text)).toBeVisible({ timeout: 10000 });
  await page.getByRole('button', { name: 'Revisar comando' }).click();

  await expect(page.getByText('Cambios por confirmar · fila 5')).toBeVisible({ timeout: 10000 });
  await expect(page.getByText('L · Frecuencia')).toBeVisible();
  expect(proposalCreated).toBe(true);

  await page.getByRole('button', { name: 'Confirmar y aplicar' }).click();

  await expect(page.getByText(/Fila 5 actualizada: J10/)).toBeVisible({ timeout: 10000 });
  expect(sheetsPutSeen).toBe(true);
  expect(sheetsPayload).toEqual({ values: [[1]] });
  expect(proposalApproved).toBe(true);

  const statusResponse = await request.get(
    `${BASE_URL}/api/device/v1/commands/${encodeURIComponent(queued.command_id)}`,
    { headers: { 'x-3c-device-token': DEVICE_TOKEN } },
  );
  expect(statusResponse.status()).toBe(200);
  const terminal = await statusResponse.json();
  expect(terminal.command.status).toBe('applied');
  expect(terminal.command.result).toContain('Fila 5 actualizada: J10');
});

test('real Monitor UI can reject a pending command and close the device state', async ({ page, request }) => {
  let proposalCreated = false;

  page.on('request', (requestEvent) => {
    const url = new URL(requestEvent.url());
    if (url.pathname === '/api/review/proposals' && requestEvent.method() === 'POST') proposalCreated = true;
  });

  await page.route('**/api/extract', async route => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({
        tarea_buscada: 'J10',
        tarea_id: '',
        columna_busqueda: 'F',
        operaciones: [{
          campo: 'frecuencia',
          columna_actualizar: 'L',
          encabezado: 'Frecuencia',
          valor_actualizar: 1,
          razon: 'CI E2E reject',
        }],
        requiere_revision: false,
      }),
    });
  });

  await page.route('https://sheets.googleapis.com/**', async route => {
    const requestEvent = route.request();
    const url = new URL(requestEvent.url());
    if (requestEvent.method() === 'PUT') {
      await route.fulfill({
        status: 500,
        contentType: 'application/json',
        body: JSON.stringify({ error: 'PUT must not happen on reject' }),
      });
      return;
    }
    const range = decodeURIComponent(url.pathname.split('/values/')[1] || '');
    if (range.includes('Data!A4:AF4')) {
      const headers = Array.from({ length: 32 }, () => '');
      headers[4] = 'TareaId';
      headers[5] = 'Nombre';
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ values: [headers] }) });
      return;
    }
    if (range.includes('Data!B5:C') || range.includes('Data!N5:O')) {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ values: [['demo', 'demo']] }) });
      return;
    }
    if (range.includes('Data!F5:F')) {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ values: [['J10']] }) });
      return;
    }
    await route.fulfill({ status: 404, contentType: 'application/json', body: JSON.stringify({ error: 'unexpected sheets range', range }) });
  });

  await page.goto(BASE_URL, { waitUntil: 'domcontentloaded' });
  const enqueue = await request.post(`${BASE_URL}/api/device/v1/commands`, {
    headers: { 'content-type': 'application/json', 'x-3c-device-token': DEVICE_TOKEN },
    data: {
      device_id: 'panel-4848s040-3c-ui-e2e-reject',
      request_id: 'panel-4848s040-3c-ui-e2e-reject-001',
      text: 'Cambia la tarea J10 a mensual',
    },
  });
  expect(enqueue.status()).toBe(202);
  const queued = await enqueue.json();

  await expect(page.getByText('Cambia la tarea J10 a mensual')).toBeVisible({ timeout: 10000 });
  await page.getByRole('button', { name: 'Revisar comando' }).click();
  await expect(page.getByText('Cambios por confirmar · fila 5')).toBeVisible({ timeout: 10000 });

  const deviceResult = page.waitForResponse(response =>
    response.url().includes(`/api/device/v1/commands/${encodeURIComponent(queued.command_id)}/result`) &&
    response.request().method() === 'POST' &&
    response.status() === 200,
  );
  await page.getByRole('button', { name: 'Cancelar' }).click();
  await deviceResult;

  expect(proposalCreated).toBe(true);

  const statusResponse = await request.get(
    `${BASE_URL}/api/device/v1/commands/${encodeURIComponent(queued.command_id)}`,
    { headers: { 'x-3c-device-token': DEVICE_TOKEN } },
  );
  expect(statusResponse.status()).toBe(200);
  const terminal = await statusResponse.json();
  expect(terminal.command.status).toBe('rejected');
});
