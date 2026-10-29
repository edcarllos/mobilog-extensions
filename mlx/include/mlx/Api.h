#pragma once
// Numbers shared with the extension SDK (sdk/include/mobilog_ext.h): the API functions reached through
// mobilog.call(fn, ...), their error codes, the events sent to mlx_event and the log levels.
// New functions get new numbers; numbers are never reused. The request and answer layouts of each
// function are documented in sdk/include/mobilog_ext.h (blocks read with WireReader, mlx/Wire.h).
#include "mlx/Manifest.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mobilog::ext {

// mobilog.call(fn, request, length, answer, capacity): answer size, or one of these (< 0).
enum ApiError : int32_t {
    kErrUnknown = -1,     // this app does not have the function (check with mobilog.has first)
    kErrDenied = -2,      // the extension was not granted the permission it needs
    kErrInvalid = -3,     // the request is malformed
    kErrLimit = -4,       // over a limit (size, count, rate)
    kErrUnavailable = -5, // not now (no simulation loaded, wrong thread...)
    kErrNotFound = -6,    // no such item (vehicle, task, layer...)
};

// API functions. 1-99: the extension itself; 100+: the map, the simulation, drawing, the scenario.
enum ApiFunction : uint32_t {
    kFnTaskResult = 1,  // tasks: the result bytes of this task
    kFnEcho = 2,        // answers with the request itself (tests, and measuring the boundary)
    kFnSubscribe = 3,   // u32 mask of events (1 << kind) sent without being asked (frames, arrivals...)

    kFnMapInfo = 100,
    kFnMapEdges = 101,
    kFnMapEdgePoints = 102,
    kFnMapNodes = 103,
    kFnMapNames = 104,
    kFnMapNearestEdge = 105,
    kFnMapNearestNode = 106,
    kFnMapPlaces = 107,
    kFnMapZones = 108,
    kFnMapNeighbourhoods = 109,
    kFnMapArea = 110,
    kFnMapEdgeRef = 111,
    kFnMapResolveEdge = 112,

    kFnRoute = 120,
    kFnRouteCostsFrom = 121,
    kFnRouteMatrix = 122,

    kFnSimInfo = 130,
    kFnSimSpeedRatios = 131,
    kFnSimPassages = 132,
    kFnSimEdgeTimes = 133,
    kFnSimVehicles = 134,

    kFnDrawSetLayer = 140,
    kFnDrawClearLayer = 141,

    kFnUiPanel = 150,   // the content of one of its panels (a UiBuffer)
    kFnUiKpis = 151,    // its indicators in the top bar
    kFnUiLegend = 152,  // its items in the map legend
    kFnUiToast = 153,   // a short message
    kFnUiChart = 154,   // the content of one of its pinnable charts (a UiBuffer)
    kFnUiTool = 155,    // turns one of its map tools on or off
    kFnUiFocus = 156,   // shows a place on the map (the user's view moves there)

    kFnScenarioSummary = 160,  // the open scenario, as the Assistant reads it (JSON)
    kFnScenarioDataGet = 161,  // its own section of the scenario (JSON)
    kFnScenarioDataSet = 162,
    kFnScenarioPropose = 163,  // actions in the Assistant's format, confirmed by the user
    kFnScenarioChange = 164,   // the same, applied at once (with undo)
    kFnScenarioUndo = 165,     // undoes its last change

    kFnVehicleCreate = 170,
    kFnVehicleDepart = 171,
    kFnVehicleRemove = 172,
    kFnVehicleStates = 173,
    kFnVehicleHide = 174,

    kFnTaskStart = 180,
    kFnTaskPoll = 181,
    kFnTaskCancel = 182,

    kFnExperimentStart = 190,
    kFnExperimentPoll = 191,
    kFnExperimentCancel = 192,

    kFnSolverDeclare = 200,  // a VRP solver it contributes (manifest: contributes solver)
    kFnSolverOffer = 201,    // solver task: a solution (the app evaluates it)
    kFnSolverParams = 202,   // solver task: the parameters of this run
    kFnSolverProgress = 203, // solver task: iterations done since the last report
};

constexpr size_t kMaxEchoBytes = 16u << 20;

// Where a function may be called.
enum ApiWhere : uint8_t { kInHooks = 1, kInTasks = 2, kInSolver = 4, kAnywhere = 7 };

struct ApiFunctionInfo {
    uint32_t fn;
    const char* name;      // what mobilog.has asks about ("map.info")
    uint32_t permission;   // 0: none needed
    uint8_t where;
};
inline constexpr ApiFunctionInfo kApiFunctions[] = {
    {kFnTaskResult, "task.result", 0, kInTasks},
    {kFnEcho, "echo", 0, kAnywhere},
    {kFnSubscribe, "events.subscribe", 0, kInHooks},
    {kFnMapInfo, "map.info", kPermMapRead, kAnywhere},
    {kFnMapEdges, "map.edges", kPermMapRead, kAnywhere},
    {kFnMapEdgePoints, "map.edge_points", kPermMapRead, kAnywhere},
    {kFnMapNodes, "map.nodes", kPermMapRead, kAnywhere},
    {kFnMapNames, "map.names", kPermMapRead, kAnywhere},
    {kFnMapNearestEdge, "map.nearest_edge", kPermMapRead, kAnywhere},
    {kFnMapNearestNode, "map.nearest_node", kPermMapRead, kAnywhere},
    {kFnMapPlaces, "map.places", kPermMapRead, kInHooks},
    {kFnMapZones, "map.zones", kPermMapRead, kInHooks},
    {kFnMapNeighbourhoods, "map.neighbourhoods", kPermMapRead, kInHooks},
    {kFnMapArea, "map.area", kPermMapRead, kInHooks},
    {kFnMapEdgeRef, "map.edge_ref", kPermMapRead, kAnywhere},
    {kFnMapResolveEdge, "map.resolve_edge", kPermMapRead, kAnywhere},
    {kFnRoute, "route.find", kPermMapRead, kAnywhere},
    {kFnRouteCostsFrom, "route.costs_from", kPermMapRead, kAnywhere},
    {kFnRouteMatrix, "route.matrix", kPermMapRead, kAnywhere},
    {kFnSimInfo, "sim.info", kPermSimRead, kInHooks},
    {kFnSimSpeedRatios, "sim.speed_ratios", kPermSimRead, kInHooks},
    {kFnSimPassages, "sim.passages", kPermSimRead, kInHooks},
    {kFnSimEdgeTimes, "sim.edge_times", kPermSimRead, kInHooks},
    {kFnSimVehicles, "sim.vehicles", kPermSimRead, kInHooks},
    {kFnDrawSetLayer, "draw.set_layer", kPermMapDraw, kInHooks},
    {kFnDrawClearLayer, "draw.clear_layer", kPermMapDraw, kInHooks},
    {kFnUiPanel, "ui.panel", 0, kInHooks},
    {kFnUiKpis, "ui.kpis", 0, kInHooks},
    {kFnUiLegend, "ui.legend", 0, kInHooks},
    {kFnUiToast, "ui.toast", 0, kInHooks},
    {kFnUiChart, "ui.chart", 0, kInHooks},
    {kFnUiTool, "ui.tool", kPermMapTools, kInHooks},
    {kFnUiFocus, "ui.focus", 0, kInHooks},
    {kFnScenarioSummary, "scenario.summary", kPermMapRead, kInHooks},
    {kFnScenarioDataGet, "scenario.data_get", 0, kInHooks},
    {kFnScenarioDataSet, "scenario.data_set", 0, kInHooks},
    {kFnScenarioPropose, "scenario.propose", kPermScenarioPropose, kInHooks},
    {kFnScenarioChange, "scenario.change", kPermScenarioWrite, kInHooks},
    {kFnScenarioUndo, "scenario.undo", kPermScenarioWrite, kInHooks},
    {kFnVehicleCreate, "vehicle.create", kPermVehicles, kInHooks},
    {kFnVehicleDepart, "vehicle.depart", kPermVehicles, kInHooks},
    {kFnVehicleRemove, "vehicle.remove", kPermVehicles, kInHooks},
    {kFnVehicleStates, "vehicle.states", kPermVehicles, kInHooks},
    {kFnVehicleHide, "vehicle.hide", kPermVehicles, kInHooks},
    {kFnTaskStart, "task.start", 0, kInHooks},
    {kFnTaskPoll, "task.poll", 0, kInHooks},
    {kFnTaskCancel, "task.cancel", 0, kInHooks},
    {kFnExperimentStart, "experiment.start", kPermSimExperiment, kInHooks},
    {kFnExperimentPoll, "experiment.poll", kPermSimExperiment, kInHooks},
    {kFnExperimentCancel, "experiment.cancel", kPermSimExperiment, kInHooks},
    {kFnSolverDeclare, "solver.declare", 0, kInHooks},
    {kFnSolverOffer, "solver.offer", 0, kInSolver},
    {kFnSolverParams, "solver.params", 0, kInSolver},
    {kFnSolverProgress, "solver.progress", 0, kInSolver},
};
const ApiFunctionInfo* apiFunction(uint32_t fn);
const ApiFunctionInfo* apiFunction(std::string_view name);
const char* apiFunctionName(uint32_t fn);  // "task.result" (what mobilog.has asks about)

// mlx_event(kind, data, length): what happened; its answer (an i32) matters only where noted. The
// data layouts are in sdk/include/mobilog_ext.h. Kinds marked "subscribed" come only after events.subscribe.
enum ApiEvent : int32_t {
    kEvFrame = 1,           // f64 seconds since the previous frame (subscribed)
    kEvPanel = 2,           // str panel: on screen this frame; send its content (ui.panel)
    kEvUi = 3,              // str panel, then the user's actions (UiEvents)
    kEvTool = 4,            // str tool, u8 action, lon/lat: answer 1 = taken (grab: the drag is the tool's)
    kEvTap = 5,             // lon/lat of a tap no tool took: answer 1 = consumed
    kEvTaskDone = 6,        // u32 task
    kEvVehicleArrived = 7,  // u32 count, then the vehicles that parked (subscribed)
    kEvNetwork = 8,         // u64 network version: the network was rebuilt (edge ids may change)
    kEvScenario = 9,        // the scenario changed
    kEvExperimentDone = 10, // u32 experiment
    kEvChart = 11,          // str chart: on screen this frame; send its content (ui.chart)
};
// kEvTool actions.
enum ApiToolAction : uint8_t { kToolTap = 0, kToolGrab, kToolDragBegin, kToolDragMove, kToolDragEnd, kToolDragCancel, kToolOff };

// Tasks started with mlx_task by the app itself (not by task.start).
enum ApiTaskKind : int32_t {
    kTaskSolveVrp = -1,  // input: str solver id, then the instance; solver.offer gives solutions. A sliced
                         // solver (it saw "_sliced" in solver.params) only starts and returns 1
    kTaskSolveStep = -2, // a sliced solver: go on for `size` ms; 1 = more, 0 = done
};
// solver.declare: the copies of this solver run in parallel (one search each, the app runs them).
inline constexpr uint32_t kSolverParallel = 0x40000000u;

}  // namespace mobilog::ext
