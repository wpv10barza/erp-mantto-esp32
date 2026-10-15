from __future__ import annotations

from typing import Annotated, Literal, TypedDict

from langchain_core.messages import AIMessage, BaseMessage
from langgraph.graph import END, START, StateGraph
from langgraph.graph.message import add_messages


class ERPState(TypedDict, total=False):
    """Conversation and command state for the ERP Mantto ESP32 workflow."""

    messages: Annotated[list[BaseMessage], add_messages]
    device_id: str
    command: str
    action: Literal["propose", "confirm", "reject"]
    normalized_command: str
    status: Literal[
        "pending_confirmation",
        "applied",
        "rejected",
        "error",
    ]
    detail: str


def _last_user_text(state: ERPState) -> str:
    command = str(state.get("command", "") or "").strip()
    if command:
        return command

    for message in reversed(state.get("messages", [])):
        content = getattr(message, "content", "")
        if isinstance(content, str) and content.strip():
            return content.strip()
    return ""


def normalize_command(state: ERPState) -> ERPState:
    command = " ".join(_last_user_text(state).split())
    if not command:
        return {
            "normalized_command": "",
            "status": "error",
            "detail": "No maintenance command was provided.",
        }

    return {
        "command": command,
        "normalized_command": command,
        "device_id": str(state.get("device_id", "") or "esp32-panel").strip(),
    }


def transition_command(state: ERPState) -> ERPState:
    if state.get("status") == "error":
        return {}

    action = str(state.get("action", "propose") or "propose").lower()

    if action == "confirm":
        return {
            "status": "applied",
            "detail": "Confirmed by human operator.",
        }

    if action == "reject":
        return {
            "status": "rejected",
            "detail": "Rejected by human operator.",
        }

    return {
        "action": "propose",
        "status": "pending_confirmation",
        "detail": "Awaiting human confirmation before any maintenance change is applied.",
    }


def respond(state: ERPState) -> ERPState:
    status = state.get("status", "error")
    command = state.get("normalized_command", "")
    device_id = state.get("device_id", "esp32-panel")
    detail = state.get("detail", "")

    text = (
        f"ERP Mantto ESP32 | device={device_id} | status={status}. "
        f"Command: {command or '[empty]'}. {detail}"
    )
    return {"messages": [AIMessage(content=text)]}


builder = StateGraph(ERPState)
builder.add_node("normalize_command", normalize_command)
builder.add_node("transition_command", transition_command)
builder.add_node("respond", respond)
builder.add_edge(START, "normalize_command")
builder.add_edge("normalize_command", "transition_command")
builder.add_edge("transition_command", "respond")
builder.add_edge("respond", END)

graph = builder.compile()
