# Device API and pending-confirmation E2E

Endpoints:

- GET /api/device/v1/health
- POST /api/device/v1/commands
- GET /api/device/v1/commands/{command_id}
- POST /api/device/v1/commands/{command_id}/confirm
- POST /api/device/v1/commands/{command_id}/reject

Required flow:

1. The device sends POST /commands.
2. The backend creates the command as pending_confirmation.
3. The device polls /commands/{command_id}.
4. A human operator confirms or rejects the command.
5. A later poll returns applied or rejected.
6. Failed backend/poll responses map to error.

The device client never calls confirm/reject. Those endpoints represent the human approval gate.

The repository test service does not write to Google Sheets. A production persistence adapter must only execute after the confirmation transition.

Run locally:

    pip install pytest
    pytest -q
