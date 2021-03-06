#include <algorithm>
#include <iostream>
#include <math.h>
#include <uWS/uWS.h>
#include "cte_eval.h"
#include "json.hpp"
#include "simple_timer.h"
#include "vehicle_controller.h"

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

/** Convert miles/hour to meters/second. */
double milesPerHour2MetersPerSecond(double x) { return x * 0.44704; }

/** Distance of one lap.
 * This was measured by taking the average of 7 laps (of drunk driving).
 */
const double distancePerLap = 8000./7.; // meters

/** Number of laps to evaluate each parameter tuning. */
const int numTuningLaps = 7;

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
std::string hasData(std::string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_last_of("]");
  if (found_null != std::string::npos) {
    return "";
  }
  else if (b1 != std::string::npos && b2 != std::string::npos) {
    return s.substr(b1, b2 - b1 + 1);
  }
  return "";
}

int main()
{
  uWS::Hub h;

  VehicleController vehicleController;
  SimpleTimer lapTimer, telemetryTimer;
  CrosstrackErrorEvaluator cteEval(false);
  double distance = -distancePerLap;
  int lapCounter = -1;
  h.onMessage([&vehicleController, &distance, &lapCounter, &lapTimer, &telemetryTimer, &cteEval](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length, uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2')
    {
      auto s = hasData(std::string(data).substr(0, length));
      if (s != "") {
        auto j = json::parse(s);
        std::string event = j[0].get<std::string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          double cte = std::stod(j[1]["cte"].get<std::string>()); // meters?
          double speed = milesPerHour2MetersPerSecond(std::stod(j[1]["speed"].get<std::string>()));
//          double angle = deg2rad(std::stod(j[1]["steering_angle"].get<std::string>()));
          double deltaTime = telemetryTimer.GetDelta();

//          std::cout << std::stod(j[1]["speed"].get<std::string>()) << " "
//                    << std::stod(j[1]["steering_angle"].get<std::string>()) << std::endl;

          distance += speed * deltaTime;
          if (distance > (distancePerLap * (double)(lapCounter + 1))) {
            const double lapTime = lapTimer.GetDelta();
            if (lapCounter == -1) {
              // Ignore first warm up lap (starting from stand still).
              std::cout << "Warm up time: " << lapTime << std::endl;
            } else {
              const int tuningLap = (lapCounter % numTuningLaps) + 1;
              std::cout << "Lap " << tuningLap << " time: " << lapTime << std::endl;
              vehicleController.SetCost(lapTime);
              if (tuningLap == numTuningLaps) {
                vehicleController.SetNextParams();
              }
            }
            ++lapCounter;
          }

          switch(cteEval.Evaluate(deltaTime, cte)) {
            case CrosstrackErrorEvaluator::DEFECTIVE:
              vehicleController.SetMode(VehicleController::RECOVERY);
              break;
            case CrosstrackErrorEvaluator::BAD:
              vehicleController.SetMode(VehicleController::SAFE);
              break;
            case CrosstrackErrorEvaluator::RISKY:
              vehicleController.SetMode(VehicleController::CAREFUL);
              break;
            case CrosstrackErrorEvaluator::OK:
              vehicleController.SetMode(VehicleController::MODERATE);
              break;
            case CrosstrackErrorEvaluator::GOOD:
              vehicleController.SetMode(VehicleController::CHALLENGING);
              break;
            case CrosstrackErrorEvaluator::IDEAL:
              vehicleController.SetMode(VehicleController::BOLD);
              break;
            default:
              assert(false); // Unknown evaluation performance.
          }

          double steerValue = vehicleController.CalcSteeringValue(deltaTime, speed, cte);
          double throttleValue = vehicleController.CalcThrottleValue(deltaTime, speed);

          // DEBUG
          json msgJson;
          msgJson["steering_angle"] = steerValue;
          msgJson["throttle"] = throttleValue;
          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          //std::cout << msg << std::endl;
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1)
    {
      res->end(s.data(), s.length());
    }
    else
    {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port))
  {
    std::cout << "Listening to port " << port << std::endl;
  }
  else
  {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
