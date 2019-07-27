#include "ExampleAIModule.h"

#include <windows.h>
#include <ctime>

#include <iostream>
#include <fstream>


using namespace BWAPI;
using namespace Filter;

static std::vector<std::string> get_tokens(std::string s) {
	size_t pos = 0;
	std::vector<std::string> tokens;
	while ((pos = s.find("_")) != std::string::npos) {
		tokens.push_back(s.substr(0, pos));
		s.erase(0, pos + 1);
	}
	tokens.push_back(s);
	return tokens;
}

static std::string get_dbname() {
	std::stringstream ss;
	auto tokens = get_tokens(Broodwar->self()->getName());

	ss << "models/" << tokens[0];
	return ss.str();
}

static std::string get_resname() {
	std::stringstream ss;
	auto tokens = get_tokens(Broodwar->self()->getName());
	std::cout << "Tokens: ";
	for (auto& token: tokens) {
		std::cout << token << " : ";
	}
	ss << "results/" << tokens[0] << "_result_" << tokens[1];
	return ss.str();
}


Action getAction(State &s, Model &m) {
	//float intention[Action::MAX_ACTION] = {};
	//for(int a = 0; a < Action::MAX_ACTION; ++a) {
	//	for (int p = 0; p < Param::MAX_PARAM; ++p) {
	//		intention[a] += m.params[a][p] * s.data[p];
	//	}
	//}
	//float rc = intention[0];
	//int indexMax = 0;
	//for (int i = 1; i < Action::MAX_ACTION; ++i) {
	//	if (rc < intention[i]) {
	//		rc = intention[i];
	//		indexMax = i;
	//	}
	//}
	auto action = argMax(m.forward(s));
	m.grads(s, action);
	return action;
}

static State createState(Unit me) {
	State s = State();
	for(auto u : Broodwar->enemy()->getUnits()) {
		if (!u->getType().isBuilding()) {
			s.data[Param::ENEMY_COUNT]++;
			s.data[Param::ENEMY_HP] += u->getHitPoints();
		}
	}
	for (auto u : Broodwar->self()->getUnits()) {
		if (!u->getType().isBuilding()) {
			s.data[Param::TEAM_COUNT]++;
			s.data[Param::TEAM_HP] += u->getHitPoints();
		}
	}
	s.data[Param::ME_HP] = static_cast<float>(me->getHitPoints());
	auto closest = me->getClosestUnit(IsEnemy && !IsBuilding);
	s.data[Param::ENEMY_DISTANCE] = closest ? closest->getPosition().getDistance(me->getPosition()) : 64.0f;
	closest = me->getClosestUnit(!IsEnemy && !IsBuilding);
	s.data[Param::TEAM_DISTANCE] = closest ? closest->getPosition().getDistance(me->getPosition()) : 64.0f;
	s.data[Param::ME_ATTACKED] = static_cast<float>(me->isUnderAttack());
	s.data[Param::ME_REPAIRED] = static_cast<float>(me->isBeingHealed());
	s.data[Param::TEAM_MINERALS] = Broodwar->self()->minerals();
	for (int i = 0; i < Param::MAX_PARAM; ++i) {
		s.edata(i) = s.data[i];
	}
	return s;
}

template <typename T>
BWAPI::Unit find_unit_in(BWAPI::Unitset us, T cb) {
	for (auto &x : us) {
		if (cb(x)) {
			return x;
		}
	}
	return nullptr;
}

template <class TContainer, typename T>
BWAPI::Unit find_max(TContainer us, T cb) {
	if (0 == us.size()) {
		return nullptr;
	}
	else if (1 == us.size()) {
		return us[0];
	}
	auto win = us.begin();
	for (auto it = us.begin() + 1; us.end() != it; ++it) {
		if (cb(*it, *win)) {
			win = it;
		}
	}
	return *win;
}

template <typename T>
BWAPI::Unit find_max_friend(T cb) {
	return find_max(Broodwar->self()->getUnits(), cb);
}

template <typename T>
BWAPI::Unit find_max_enemy(T cb) {
	return find_max(Broodwar->enemy()->getUnits(), cb);
}

template <typename T>
BWAPI::Unit find_friend(T cb) {
	return find_unit_in(Broodwar->self()->getUnits(), cb);
}

template <typename T>
BWAPI::Unit find_enemy(T cb) {
	return find_unit_in(Broodwar->enemy()->getUnits(), cb);
}

template <typename T>
std::vector<BWAPI::Unit> filter_units(BWAPI::Unitset us, T cb) {
	std::vector<BWAPI::Unit> out;
	for (auto &ref : us) {
		if (cb(ref)) {
			out.emplace_back(ref);
		}
	}
	return out;
}

template <typename T>
std::vector<BWAPI::Unit> filter_friendly(T cb) {
	return filter_units(Broodwar->self()->getUnits(), cb);
}

template <typename T>
std::vector<BWAPI::Unit> filter_enemy(T cb) {
	return filter_units(Broodwar->enemy()->getUnits(), cb);
}

static void commitAction(Action a, Unit me, bool debug) {
	if (Action::ATTACK == a) {
		if(debug)
			std::cout << "MY QUEST IS TO ATTACK" << std::endl;
		//auto enemyUnits{ filter_enemy([](auto u) {
		//	return !u->getType().isBuilding();
		//})};
		//auto target = find_max(enemyUnits, [](Unit left, Unit right) {
		//	return left->getHitPoints() < right->getHitPoints();
		//});
		auto target = me->getClosestUnit(IsEnemy && !IsBuilding);
		if (target) {
			me->attack(target);
			Broodwar->drawLineMap(me->getPosition(), target->getPosition(), Color(255, 0, 0));
		}
	}
	else if (Action::REPAIR == a) {
		if(debug)
			std::cout << "MY QUEST IS TO REPAIR" << std::endl;
		auto friendlyUnits{ filter_friendly([](auto u) {
			return !u->getType().isBuilding();
		}) };
		auto target = find_max(friendlyUnits, [](Unit left, Unit right) {
			return left->getHitPoints() < right->getHitPoints();
		});
		if (target) {
			me->repair(target);
			Broodwar->drawLineMap(me->getPosition(), target->getPosition(), Color(0, 255, 0));
		}
	}
	else if (Action::FLEE == a) {
		if(debug)
			std::cout << "MY QUEST IS TO RUN AWAAAAY!!!" << std::endl;
		auto target = me->getClosestUnit(IsEnemy && !IsBuilding);
		if (target) {
			auto delta = target->getPosition() - me->getPosition();
			auto move_target = me->getPosition() + delta * (-1);
			Broodwar->drawLineMap(me->getPosition(), move_target, Color(0, 0, 255));
			me->move(move_target);
		}
	}

}


void ExampleAIModule::onStart()
{
	debug = false;
	char buf[255] = {};
	if (0 != GetEnvironmentVariableA("DEBUG", buf, sizeof(buf))) {
		debug = true;
	}
	std::cout << "Loading model at " << get_dbname() << std::endl;
	if(!loadModel(model, get_dbname())) {
		Broodwar->sendText("My pants are on my head!");
	}
	for (auto &action : model.params) {
		for (auto &p : action) {
			std::cout << p << " ";
		}
		std::cout << std::endl;
	}
  // Hello World!
  Broodwar->sendText("Hello world!");

  // Print the map name.
  // BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
  Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

  // Enable the UserInput flag, which allows us to control the bot and type messages.
  Broodwar->enableFlag(Flag::UserInput);

  // Uncomment the following line and the bot will know about everything through the fog of war (cheat).
  Broodwar->enableFlag(Flag::CompleteMapInformation);

  // Set the command optimization level so that common commands can be grouped
  // and reduce the bot's APM (Actions Per Minute).
  Broodwar->setCommandOptimizationLevel(2);
  Broodwar->setLocalSpeed(0);

  // Check if this is a replay
  if ( Broodwar->isReplay() )
  {

    // Announce the players in the replay
    Broodwar << "The following players are in this replay:" << std::endl;
    
    // Iterate all the players in the game using a std:: iterator
    Playerset players = Broodwar->getPlayers();
    for(auto p : players)
    {
      // Only print the player if they are not an observer
      if ( !p->isObserver() )
        Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
    }
  }
  else // if this is not a replay
  {
    // Retrieve you and your enemy's races. enemy() will just return the first enemy.
    // If you wish to deal with multiple enemies then you must use enemies().
    if ( Broodwar->enemy() ) // First make sure there is an enemy
      Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
  }
  start_time = time(NULL);
}

void ExampleAIModule::onEnd(bool isWinner)
{
	//char buf[MAX_PATH] = {};
	//if (0 != GetEnvironmentVariableA("resultfile", buf, sizeof(buf)))
	//{
	//	std::ofstream out(buf);
	//	out << (isWinner ? "1" : "0");
	//}
	model.winner = isWinner;
	std::cout << "THE END! I AM " << (isWinner ? "WINNER" : "LOOSER") << std::endl;
	saveModel(model, get_resname());
}

void ExampleAIModule::onFrame()
{
  // Called once every game frame

  // Display the game frame rate as text in the upper left area of the screen
  Broodwar->drawTextScreen(200, 0,  "FPS: %d", Broodwar->getFPS() );
  Broodwar->drawTextScreen(200, 20, "Average FPS: %f", Broodwar->getAverageFPS() );

  // Return if the game is a replay or is paused
  if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
    return;

  // Prevent spamming by only running our onFrame once every number of latency frames.
  // Latency frames are the number of frames before commands are processed.
  if ( Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0 )
    return;

  bool did_lose = true;

  // Iterate through all the units that we own
  for (auto &u : Broodwar->self()->getUnits())
  {
    // Ignore the unit if it no longer exists
    // Make sure to include this block when handling any Unit pointer!
    if ( !u->exists() )
      continue;

    // Ignore the unit if it has one of the following status ailments
    if ( u->isLockedDown() || u->isMaelstrommed() || u->isStasised() )
      continue;

    // Ignore the unit if it is in one of the following states
    if ( u->isLoaded() || !u->isPowered() || u->isStuck() )
      continue;

    // Ignore the unit if it is incomplete or busy constructing
    if ( !u->isCompleted() || u->isConstructing() )
      continue;


    // Finally make the unit do some stuff!


    // If the unit is a worker unit
    if ( u->getType().isWorker() )
    {
		did_lose = false;
      // if our worker is idle
      // if ( u->isIdle() )
      {
		  auto s{ createState(u) };
		  auto action{ getAction(s, model) };

		  commitAction(action, u, debug);

        // Order workers carrying a resource to return them to the center,
        // otherwise find a mineral patch to harvest.
        //if ( u->isCarryingGas() || u->isCarryingMinerals() )
        //{
        //  u->returnCargo();
        //}
        //else if ( !u->getPowerUp() )  // The worker cannot harvest anything if it
        //{                             // is carrying a powerup such as a flag
        //  // Harvest from the nearest mineral patch or gas refinery
        //  if ( !u->gather( u->getClosestUnit( IsMineralField || IsRefinery )) )
        //  {
        //    // If the call fails, then print the last error message
        //    Broodwar << Broodwar->getLastError() << std::endl;
        //  }

        //} // closure: has no powerup
      } // closure: if idle

    }
    //else if ( u->getType().isResourceDepot() ) // A resource depot is a Command Center, Nexus, or Hatchery
    //{

    //  // Order the depot to construct more workers! But only when it is idle.
    //  if ( u->isIdle() && !u->train(u->getType().getRace().getWorker()) )
    //  {
    //    // If that fails, draw the error at the location so that you can visibly see what went wrong!
    //    // However, drawing the error once will only appear for a single frame
    //    // so create an event that keeps it on the screen for some frames
    //    Position pos = u->getPosition();
    //    Error lastErr = Broodwar->getLastError();
    //    Broodwar->registerEvent([pos,lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
    //                            nullptr,    // condition
    //                            Broodwar->getLatencyFrames());  // frames to run

    //    // Retrieve the supply provider type in the case that we have run out of supplies
    //    UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
    //    static int lastChecked = 0;

    //    // If we are supply blocked and haven't tried constructing more recently
    //    if (  lastErr == Errors::Insufficient_Supply &&
    //          lastChecked + 400 < Broodwar->getFrameCount() &&
    //          Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0 )
    //    {
    //      lastChecked = Broodwar->getFrameCount();

    //      // Retrieve a unit that is capable of constructing the supply needed
    //      Unit supplyBuilder = u->getClosestUnit(  GetType == supplyProviderType.whatBuilds().first &&
    //                                                (IsIdle || IsGatheringMinerals) &&
    //                                                IsOwned);
    //      // If a unit was found
    //      if ( supplyBuilder )
    //      {
    //        if ( supplyProviderType.isBuilding() )
    //        {
    //          TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
    //          if ( targetBuildLocation )
    //          {
    //            // Register an event that draws the target build location
    //            Broodwar->registerEvent([targetBuildLocation,supplyProviderType](Game*)
    //                                    {
    //                                      Broodwar->drawBoxMap( Position(targetBuildLocation),
    //                                                            Position(targetBuildLocation + supplyProviderType.tileSize()),
    //                                                            Colors::Blue);
    //                                    },
    //                                    nullptr,  // condition
    //                                    supplyProviderType.buildTime() + 100 );  // frames to run

    //            // Order the builder to construct the supply structure
    //            supplyBuilder->build( supplyProviderType, targetBuildLocation );
    //          }
    //        }
    //        else
    //        {
    //          // Train the supply provider (Overlord) if the provider is not a structure
    //          supplyBuilder->train( supplyProviderType );
    //        }
    //      } // closure: supplyBuilder is valid
    //    } // closure: insufficient supply
    //  } // closure: failed to train idle unit

    //}

  } // closure: unit iterator

  if (did_lose) {
	  Broodwar->sendText("No my workers are gone. Honorable soduku.");
	  Broodwar->leaveGame();
  }
  double elapsed = difftime(time(NULL), start_time);
  std::cout << "@ ELAPSED " << elapsed << std::endl;
  if (elapsed > 10) {
	  std::cout << "@@@ Timeout!" << std::endl;
	  Broodwar->sendText("timeout");
	  Broodwar->leaveGame();
  }

}

void ExampleAIModule::onSendText(std::string text)
{

  // Send the text to the game if it is not being processed.
  Broodwar->sendText("%s", text.c_str());


  // Make sure to use %s and pass the text as a parameter,
  // otherwise you may run into problems when you use the %(percent) character!

}

void ExampleAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
  // Parse the received text
  Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
  if (text == "timeout") {
	  Broodwar->leaveGame();
  }
}

void ExampleAIModule::onPlayerLeft(BWAPI::Player player)
{
  // Interact verbally with the other players in the game by
  // announcing that the other player has left.
  Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{

  // Check if the target is a valid position
  if ( target )
  {
    // if so, print the location of the nuclear strike target
    Broodwar << "Nuclear Launch Detected at " << target << std::endl;
  }
  else 
  {
    // Otherwise, ask other players where the nuke is!
    Broodwar->sendText("Where's the nuke?");
  }

  // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void ExampleAIModule::onUnitDiscover(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitShow(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitCreate(BWAPI::Unit unit)
{
  if ( Broodwar->isReplay() )
  {
    // if we are in a replay, then we will print out the build order of the structures
    if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
    {
      int seconds = Broodwar->getFrameCount()/24;
      int minutes = seconds/60;
      seconds %= 60;
      Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
    }
  }
}

void ExampleAIModule::onUnitDestroy(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitMorph(BWAPI::Unit unit)
{
  if ( Broodwar->isReplay() )
  {
    // if we are in a replay, then we will print out the build order of the structures
    if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
    {
      int seconds = Broodwar->getFrameCount()/24;
      int minutes = seconds/60;
      seconds %= 60;
      Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
    }
  }
}

void ExampleAIModule::onUnitRenegade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onSaveGame(std::string gameName)
{
  Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void ExampleAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
