/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TrackDesignAction.h"

#include "../Context.h"
#include "../management/Finance.h"
#include "../management/Research.h"
#include "../object/ObjectManager.h"
#include "../object/ObjectRepository.h"
#include "../ride/TrackDesign.h"
#include "RideCreateAction.h"
#include "RideDemolishAction.h"
#include "RideSetNameAction.h"
#include "RideSetSettingAction.h"
#include "RideSetVehicleAction.h"

static int32_t place_virtual_track(
    const TrackDesign& td6, uint8_t ptdOperation, bool placeScenery, Ride* ride, const CoordsXYZ& loc)
{
    return place_virtual_track(const_cast<TrackDesign*>(&td6), ptdOperation, placeScenery, ride, loc);
}

TrackDesignActionResult::TrackDesignActionResult()
    : GameActions::Result(GameActions::Status::Ok, STR_NONE)
{
}

TrackDesignActionResult::TrackDesignActionResult(GameActions::Status error)
    : GameActions::Result(error, STR_RIDE_CONSTRUCTION_CANT_CONSTRUCT_THIS_HERE, STR_NONE)
{
}

TrackDesignActionResult::TrackDesignActionResult(GameActions::Status error, rct_string_id title, rct_string_id message)
    : GameActions::Result(error, title, message)
{
}

TrackDesignActionResult::TrackDesignActionResult(GameActions::Status error, rct_string_id message)
    : GameActions::Result(error, STR_RIDE_CONSTRUCTION_CANT_CONSTRUCT_THIS_HERE, message)
{
}

TrackDesignAction::TrackDesignAction(const CoordsXYZD& location, const TrackDesign& td)
    : _loc(location)
    , _td(td)
{
}

void TrackDesignAction::AcceptParameters(GameActionParameterVisitor& visitor)
{
    visitor.Visit(_loc);
    // TODO visit the track design (it has a lot of sub fields)
}

uint16_t TrackDesignAction::GetActionFlags() const
{
    return GameActionBase::GetActionFlags();
}

void TrackDesignAction::Serialise(DataSerialiser& stream)
{
    GameAction::Serialise(stream);

    stream << DS_TAG(_loc);
    _td.Serialise(stream);
}

GameActions::Result::Ptr TrackDesignAction::Query() const
{
    auto res = MakeResult();
    res->Position.x = _loc.x + 16;
    res->Position.y = _loc.y + 16;
    res->Position.z = _loc.z;
    res->Expenditure = ExpenditureType::RideConstruction;
    _currentTrackPieceDirection = _loc.direction;

    if (!LocationValid(_loc))
    {
        return MakeResult(GameActions::Status::InvalidParameters);
    }

    const rct_object_entry* rideEntryObject = &_td.vehicle_object;

    ObjectType entryType;
    ObjectEntryIndex entryIndex;
    if (!find_object_in_entry_group(rideEntryObject, &entryType, &entryIndex))
    {
        entryIndex = OBJECT_ENTRY_INDEX_NULL;
    }
    // Force a fallback if the entry is not invented yet a td6 of it is selected, which can happen in select-by-track-type mode.
    else if (!ride_entry_is_invented(entryIndex) && !gCheatsIgnoreResearchStatus)
    {
        entryIndex = OBJECT_ENTRY_INDEX_NULL;
    }

    // Colours do not matter as will be overwritten
    auto rideCreateAction = RideCreateAction(_td.type, entryIndex, 0, 0);
    rideCreateAction.SetFlags(GetFlags());
    auto r = GameActions::ExecuteNested(&rideCreateAction);
    auto rideIndex = static_cast<RideCreateGameActionResult*>(r.get())->rideIndex;

    if (r->Error != GameActions::Status::Ok)
    {
        return MakeResult(GameActions::Status::NoFreeElements, STR_CANT_CREATE_NEW_RIDE_ATTRACTION, STR_NONE);
    }

    auto ride = get_ride(rideIndex);
    if (ride == nullptr)
    {
        log_warning("Invalid game command for track placement, ride id = %d", rideIndex);
        return MakeResult(GameActions::Status::Unknown);
    }

    money32 cost = 0;

    bool placeScenery = true;
    cost = place_virtual_track(_td, PTD_OPERATION_PLACE_QUERY, placeScenery, ride, _loc);
    if (_trackDesignPlaceStateSceneryUnavailable)
    {
        placeScenery = false;
        cost = place_virtual_track(_td, PTD_OPERATION_PLACE_QUERY, placeScenery, ride, _loc);
    }

    rct_string_id error_reason = gGameCommandErrorText;
    auto gameAction = RideDemolishAction(ride->id, RIDE_MODIFY_DEMOLISH);
    gameAction.SetFlags(GetFlags());

    GameActions::ExecuteNested(&gameAction);
    if (cost == MONEY32_UNDEFINED)
    {
        return MakeResult(GameActions::Status::Disallowed, error_reason);
    }
    res->Cost = cost;
    return res;
}

GameActions::Result::Ptr TrackDesignAction::Execute() const
{
    auto res = MakeResult();
    res->Position.x = _loc.x + 16;
    res->Position.y = _loc.y + 16;
    res->Position.z = _loc.z;
    res->Expenditure = ExpenditureType::RideConstruction;

    const rct_object_entry* rideEntryObject = &_td.vehicle_object;

    ObjectType entryType;
    ObjectEntryIndex entryIndex;
    if (!find_object_in_entry_group(rideEntryObject, &entryType, &entryIndex))
    {
        entryIndex = OBJECT_ENTRY_INDEX_NULL;
    }
    // Force a fallback if the entry is not invented yet a td6 of it is selected, which can happen in select-by-track-type mode.
    else if (!ride_entry_is_invented(entryIndex) && !gCheatsIgnoreResearchStatus)
    {
        entryIndex = OBJECT_ENTRY_INDEX_NULL;
    }

    // Colours do not matter as will be overwritten
    auto rideCreateAction = RideCreateAction(_td.type, entryIndex, 0, 0);
    rideCreateAction.SetFlags(GetFlags());
    auto r = GameActions::ExecuteNested(&rideCreateAction);
    auto rideIndex = static_cast<RideCreateGameActionResult*>(r.get())->rideIndex;

    if (r->Error != GameActions::Status::Ok)
    {
        return MakeResult(GameActions::Status::NoFreeElements, STR_CANT_CREATE_NEW_RIDE_ATTRACTION, STR_NONE);
    }

    auto ride = get_ride(rideIndex);
    if (ride == nullptr)
    {
        log_warning("Invalid game command for track placement, ride id = %d", rideIndex);
        return MakeResult(GameActions::Status::Unknown);
    }

    money32 cost = 0;

    bool placeScenery = true;
    cost = place_virtual_track(_td, PTD_OPERATION_PLACE_QUERY, placeScenery, ride, _loc);
    if (_trackDesignPlaceStateSceneryUnavailable)
    {
        placeScenery = false;
        cost = place_virtual_track(_td, PTD_OPERATION_PLACE_QUERY, placeScenery, ride, _loc);
    }

    if (cost != MONEY32_UNDEFINED)
    {
        uint8_t operation;
        if (GetFlags() & GAME_COMMAND_FLAG_GHOST)
        {
            operation = PTD_OPERATION_PLACE_GHOST;
        }
        else
        {
            operation = PTD_OPERATION_PLACE;
        }
        if (GetFlags() & GAME_COMMAND_FLAG_REPLAY)
        {
            operation |= PTD_OPERATION_FLAG_IS_REPLAY;
        }
        cost = place_virtual_track(_td, operation, placeScenery, ride, _loc);
    }

    if (cost == MONEY32_UNDEFINED)
    {
        rct_string_id error_reason = gGameCommandErrorText;
        auto gameAction = RideDemolishAction(ride->id, RIDE_MODIFY_DEMOLISH);
        gameAction.SetFlags(GetFlags());

        GameActions::ExecuteNested(&gameAction);
        return MakeResult(GameActions::Status::Disallowed, error_reason);
    }

    if (entryIndex != OBJECT_ENTRY_INDEX_NULL)
    {
        auto colour = ride_get_unused_preset_vehicle_colour(entryIndex);
        auto rideSetVehicleAction = RideSetVehicleAction(ride->id, RideSetVehicleType::RideEntry, entryIndex, colour);
        GameActions::ExecuteNested(&rideSetVehicleAction);
    }

    set_operating_setting_nested(ride->id, RideSetSetting::Mode, static_cast<uint8_t>(_td.ride_mode), GAME_COMMAND_FLAG_APPLY);
    auto rideSetVehicleAction2 = RideSetVehicleAction(ride->id, RideSetVehicleType::NumTrains, _td.number_of_trains);
    GameActions::ExecuteNested(&rideSetVehicleAction2);

    auto rideSetVehicleAction3 = RideSetVehicleAction(
        ride->id, RideSetVehicleType::NumCarsPerTrain, _td.number_of_cars_per_train);
    GameActions::ExecuteNested(&rideSetVehicleAction3);

    set_operating_setting_nested(ride->id, RideSetSetting::Departure, _td.depart_flags, GAME_COMMAND_FLAG_APPLY);
    set_operating_setting_nested(ride->id, RideSetSetting::MinWaitingTime, _td.min_waiting_time, GAME_COMMAND_FLAG_APPLY);
    set_operating_setting_nested(ride->id, RideSetSetting::MaxWaitingTime, _td.max_waiting_time, GAME_COMMAND_FLAG_APPLY);
    set_operating_setting_nested(ride->id, RideSetSetting::Operation, _td.operation_setting, GAME_COMMAND_FLAG_APPLY);
    set_operating_setting_nested(ride->id, RideSetSetting::LiftHillSpeed, _td.lift_hill_speed & 0x1F, GAME_COMMAND_FLAG_APPLY);

    uint8_t num_circuits = _td.num_circuits;
    if (num_circuits == 0)
    {
        num_circuits = 1;
    }
    set_operating_setting_nested(ride->id, RideSetSetting::NumCircuits, num_circuits, GAME_COMMAND_FLAG_APPLY);
    ride->SetToDefaultInspectionInterval();
    ride->lifecycle_flags |= RIDE_LIFECYCLE_NOT_CUSTOM_DESIGN;
    ride->colour_scheme_type = _td.colour_scheme;

    ride->entrance_style = _td.entrance_style;

    for (int32_t i = 0; i < RCT12_NUM_COLOUR_SCHEMES; i++)
    {
        ride->track_colour[i].main = _td.track_spine_colour[i];
        ride->track_colour[i].additional = _td.track_rail_colour[i];
        ride->track_colour[i].supports = _td.track_support_colour[i];
    }

    for (size_t i = 0; i <= MAX_VEHICLES_PER_RIDE; i++)
    {
        auto tdIndex = std::min(i, std::size(_td.vehicle_colours) - 1);
        ride->vehicle_colours[i].Body = _td.vehicle_colours[tdIndex].body_colour;
        ride->vehicle_colours[i].Trim = _td.vehicle_colours[tdIndex].trim_colour;
        ride->vehicle_colours[i].Ternary = _td.vehicle_additional_colour[tdIndex];
    }

    for (int32_t count = 1; count == 1 || r->Error != GameActions::Status::Ok; ++count)
    {
        auto name = count == 1 ? _td.name : (_td.name + " " + std::to_string(count));
        auto gameAction = RideSetNameAction(ride->id, name);
        gameAction.SetFlags(GetFlags());
        r = GameActions::ExecuteNested(&gameAction);
    }
    res->Cost = cost;
    res->rideIndex = ride->id;
    return res;
}
