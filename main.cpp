#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <limits>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <set>

using namespace std;

// Constants
const double INF = numeric_limits<double>::max();
const double EPSILON = 1e-9; // 0.000000001

//------------------HELPER FUNCTIONS------------------------
// Simple JSON parser helper functions
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\",");
    return str.substr(first, (last - first + 1));
}

string extractValue(const string& line, const string& key) {
    size_t pos = line.find("\"" + key + "\"");
    if (pos == string::npos) return "";

    pos = line.find(":", pos);
    if (pos == string::npos) return "";

    size_t start = line.find_first_not_of(" \t:", pos);
    if (start == string::npos) return "";

    size_t end;
    if (line[start] == '"') {
        start++;
        end = line.find('"', start);
    }
    else {
        end = line.find_first_of(",}", start);
    }

    return trim(line.substr(start, end - start));
}

// Helper: extract a quoted string value from a JSON object
string extractStringValue(const string& json, const string& key, size_t objectStart)
{
    // Find the key inside the current object
    string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey, objectStart);
    if (keyPos == string::npos) return "";

    // Move to the colon after the key
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == string::npos) return "";

    // Skip whitespace and the opening quote
    size_t valStart = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valStart == string::npos || json[valStart] != '"') return "";

    ++valStart;                                   // skip the opening "
    size_t valEnd = json.find('"', valStart);     // closing quote
    if (valEnd == string::npos) return "";

    return json.substr(valStart, valEnd - valStart);
}

// Helper: extract a numeric value from a JSON object
double extractNumericValue(const string& json, const string& key, size_t objectStart)
{
    // Find the key inside the current object
    string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey, objectStart);
    if (keyPos == string::npos) return 0.0;

    // Move to the colon after the key
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == string::npos) return 0.0;

    // Skip whitespace
    size_t valStart = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valStart == string::npos) return 0.0;

    // Find the end of the number (comma, } or ] )
    size_t valEnd = json.find_first_of(",}]", valStart);
    if (valEnd == string::npos) valEnd = json.size();

    string numStr = json.substr(valStart, valEnd - valStart);
    // Trim any trailing whitespace that might have slipped in
    numStr = trim(numStr);

    try {
        return stod(numStr);
    }
    catch (...) {
        return 0.0;
    }
}
//------------------EOF HELPER FUNCTIONS------------------------

//--------------------DATA STRUCTURES---------------------------

// City structure
struct City {
    string code;
    string name;
    string airportName;
    string country;
    string timezone;
    double latitude;
    double longitude;

    City() : latitude(0), longitude(0) {}
};

// Flight structure (Edge in graph)
struct Flight {
    string flightNo;
    string destination;
    double duration;  // in hours
    double cost;      // in dollars
    string airline;
    string departureTime;
    string arrivalTime;
    string aircraft;
    int seatsAvailable;

    Flight() : duration(0), cost(0), seatsAvailable(0) {}

    Flight(string dest, string fNo, double dur, double c, string air,
        string depTime = "", string arrTime = "", string craft = "", int seats = 0)
        : destination(dest), flightNo(fNo), duration(dur), cost(c),
        airline(air), departureTime(depTime), arrivalTime(arrTime),
        aircraft(craft), seatsAvailable(seats) {
    }
};

// Route structure (stores complete path)
struct Route {
    vector<string> cities;
    vector<Flight> flights;
    double totalCost;
    double totalDuration;
    int stops;

    Route() : totalCost(0), totalDuration(0), stops(0) {}
};

// Priority queue element for Dijkstra's
struct PQNode {
    string city;
    double cost;
    double duration;

    // Standard Dijkstra's uses cost only for ordering
    bool operator>(const PQNode& other) const {
        return cost > other.cost; // Min heap based on cost
    }
};

// Label structure for Multi-Objective Dijkstra (stores path properties)
struct Label {
    double cost;
    double duration;
    string parentCity;
    Flight parentFlight;

    // Default constructor for map initialization
    Label() : cost(INF), duration(INF) {}

    // Check if *this* label dominates *other*.
    // A dominates B if A is better in ALL criteria and strictly better in at least one.
    bool dominates(const Label& other) const {
        return cost <= other.cost && duration <= other.duration && (cost < other.cost || duration < other.duration);
    }

    // Check if the current label is dominated by *other*.
    bool isDominatedBy(const Label& other) const {
        return other.dominates(*this);
    }

    // Equality check for duplicate labels
    bool operator==(const Label& other) const {
        return cost == other.cost && duration == other.duration;
    }
};

// Priority Queue element for Multi-Objective search
// The comparison can use a weighted sum (heuristic) or just one criterion.
// We'll use a simple sum (Cost + Duration) as a heuristic to guide the search.
struct PQElement {
    string city;
    double cost;
    double duration;
    double heuristicSum; // cost + duration

    // Constructor for convenience
    PQElement(string c, double co, double du)
        : city(c), cost(co), duration(du), heuristicSum(co + du) {
    }

    // Min heap based on the heuristic sum
    bool operator>(const PQElement& other) const {
        return heuristicSum > other.heuristicSum;
    }
};
// =========================================================

//--------------------EOF DATA STRUCTURES---------------------------

// CLI Interface
void displayMenu() {
    cout << "\n--------------------------------------------------\n";
    cout << "|      AIRLINE BOOKING SYSTEM                     |\n";
    cout << "--------------------------------------------------\n";
    cout << "1. Search Flights (Cheapest Route)\n";
    cout << "2. Search Flights (Fastest Route)\n";
    cout << "3. Search Flights (Minimum Stops)\n";
    cout << "4. Search Flights (Pareto-Optimal Routes)  <-- NEW\n";
    cout << "5. Compare All Three Optimal Options\n";
    cout << "6. Display Network Stats\n";
    cout << "7. List All Cities\n";
    cout << "8. City Information\n";
    cout << "9. Display ENTIRE Flight Graph\n";
    cout << "0. Exit\n";
    cout << string(48, '-') << "\n";
    cout << "Enter choice: ";
}

void reconstructAllPaths(
    const string& currentCity,
    const string& source,
    const unordered_map<string, vector<pair<string, Flight>>>& parentCandidates,
    vector<Route>& finalRoutes,
    Route currentRoute // Passed by value (copied)
) {
    // 1. Base Case: Reached the source city
    if (currentCity == source) {
        // Add the source city, finalize, and save the route.
        currentRoute.cities.push_back(source);

        // Reverse to get the chronological path (Source -> Destination)
        reverse(currentRoute.cities.begin(), currentRoute.cities.end());
        reverse(currentRoute.flights.begin(), currentRoute.flights.end());

        // Recalculate totals (optional but good practice)
        currentRoute.totalCost = 0;
        currentRoute.totalDuration = 0;
        for (const Flight& f : currentRoute.flights) {
            currentRoute.totalCost += f.cost;
            currentRoute.totalDuration += f.duration;
        }
        currentRoute.stops = currentRoute.flights.size(); // stops = number of flights - 1, but this is fixed later
        currentRoute.stops = max(0, (int)currentRoute.flights.size() - 1);

        finalRoutes.push_back(currentRoute);
        return;
    }

    if (parentCandidates.count(currentCity) == 0) {
        return;
    }

    // 2. Recursive Step: Try every optimal parent candidate
    for (const auto& candidate : parentCandidates.at(currentCity)) {
        string parentCity = candidate.first;
        Flight flight = candidate.second;

        // Create a new route object for the recursive call
        Route nextRoute = currentRoute;

        // Store the city and flight *segment* in reverse order (Destination <- Source)
        nextRoute.cities.push_back(currentCity);
        nextRoute.flights.push_back(flight);

        // Recurse to the parent city
        reconstructAllPaths(parentCity, source, parentCandidates, finalRoutes, nextRoute);
    }
}


// Main Flight Graph class
class FlightGraph {
private:
    unordered_map<string, vector<Flight>> adjList;
    unordered_map<string, City> cities;

public:
    // Add a flight to the graph (unchanged)
    void addFlight(string source, string dest, string flightNo,
        double duration, double cost, string airline,
        string depTime = "", string arrTime = "",
        string aircraft = "", int seats = 0) {
        adjList[source].push_back(Flight(dest, flightNo, duration, cost, airline,
            depTime, arrTime, aircraft, seats));
    }

    // Add city information (unchanged)
    void addCity(const City& city) {
        cities[city.code] = city;
    }

    // Load cities from JSON file (unchanged - kept for completeness)
    bool loadCitiesFromJSON(const string& filename) {
        // Read entire file into string
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open " << filename << endl;
            cerr << "   Make sure the file exists in the current directory.\n";
            return false;
        }

        // Read entire file content
        stringstream buffer;
        buffer << file.rdbuf();
        string content = buffer.str();
        file.close();

        if (content.empty()) {
            cerr << "Error: File is empty\n";
            return false;
        }

        cout << " Loading cities from " << filename << "...\n";
        cout << "   File size: " << content.length() << " bytes\n";

        // Find the cities array
        size_t citiesPos = content.find("\"cities\"");
        if (citiesPos == string::npos) {
            cerr << "Error: Could not find 'cities' array in JSON\n";
            return false;
        }

        size_t arrayStart = content.find("[", citiesPos);
        if (arrayStart == string::npos) {
            cerr << "Error: Could not find cities array start '['\n";
            return false;
        }

        cout << "   Found cities array\n";

        int cityCount = 0;
        size_t pos = arrayStart;

        // Find each city object
        while (true) {
            size_t objectStart = content.find("{", pos);
            if (objectStart == string::npos) break;

            size_t objectEnd = content.find("}", objectStart);
            if (objectEnd == string::npos) break;

            // Check if we're past the cities array
            size_t arrayEnd = content.find("]", pos);
            if (arrayEnd != string::npos && objectStart > arrayEnd) break;

            // Extract city data within this object
            City city;
            city.code = extractStringValue(content, "code", objectStart);
            city.name = extractStringValue(content, "name", objectStart);
            city.airportName = extractStringValue(content, "airport_name", objectStart);
            city.country = extractStringValue(content, "country", objectStart);
            city.timezone = extractStringValue(content, "timezone", objectStart);
            city.latitude = extractNumericValue(content, "latitude", objectStart);
            city.longitude = extractNumericValue(content, "longitude", objectStart);

            // Validate essential fields
            if (!city.code.empty() && !city.name.empty()) {
                cities[city.code] = city;          // store in the graph's city map
                cityCount++;
                // cout << " Loaded: " << city.code << " - " << city.name << endl; // Commented for cleaner output
            }
            else {
                cerr << " Warning: Skipped incomplete city at position " << objectStart << endl;
            }

            pos = objectEnd + 1;
        }

        cout << "\n Successfully loaded " << cityCount << " cities\n\n";
        return true;
    }


    // Load flights from JSON file (unchanged - kept for completeness)
    bool loadFlightsFromJSON(const string& filename) {
        // Read entire file into string
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open " << filename << endl;
            cerr << " Make sure the file exists in the current directory.\n";
            return false;
        }

        // Read entire file content
        stringstream buffer;
        buffer << file.rdbuf();
        string content = buffer.str();
        file.close();

        if (content.empty()) {
            cerr << "Error: File is empty\n";
            return false;
        }

        cout << "Loading flights from " << filename << "...\n";
        cout << " File size: " << content.length() << " bytes\n";

        // Find the flights array
        size_t flightsPos = content.find("\"flights\"");
        if (flightsPos == string::npos) {
            cerr << "Error: Could not find 'flights' array in JSON\n";
            return false;
        }

        size_t arrayStart = content.find("[", flightsPos);
        if (arrayStart == string::npos) {
            cerr << "Error: Could not find flights array start '['\n";
            return false;
        }

        cout << " Found flights array\n";

        int flightCount = 0;
        size_t pos = arrayStart;

        // Find each flight object
        while (true) {
            size_t objectStart = content.find("{", pos);
            if (objectStart == string::npos) break;
            size_t objectEnd = content.find("}", objectStart);
            if (objectEnd == string::npos) break;

            // Check if we're past the flights array
            size_t arrayEnd = content.find("]", pos);
            if (arrayEnd != string::npos && objectStart > arrayEnd) break;

            // Extract flight data within this object
            string source = extractStringValue(content, "source", objectStart);
            string destination = extractStringValue(content, "destination", objectStart);
            string flightNo = extractStringValue(content, "flight_number", objectStart);
            string airline = extractStringValue(content, "airline", objectStart);
            string depTime = extractStringValue(content, "departure_time", objectStart);
            string arrTime = extractStringValue(content, "arrival_time", objectStart);
            string aircraft = extractStringValue(content, "aircraft", objectStart);

            double duration = extractNumericValue(content, "duration_hours", objectStart);
            double cost = extractNumericValue(content, "cost_usd", objectStart);
            int seats = (int)extractNumericValue(content, "seats_available", objectStart);

            // Validate essential fields
            if (!source.empty() && !destination.empty() && !flightNo.empty()) {
                addFlight(source, destination, flightNo, duration, cost, airline,
                    depTime, arrTime, aircraft, seats);
                flightCount++;
                // cout << " Loaded: " << source << " to " << destination << " (" << flightNo << ")" << endl; // Commented for cleaner output
            }
            else {
                cerr << " Warning: Skipped incomplete flight at position " << objectStart << endl;
            }

            pos = objectEnd + 1;
        }

        if (flightCount > 0) {
            cout << "\n Successfully loaded " << flightCount << " flights\n\n";
            return true;
        }
        else {
            cerr << "\n No flights were loaded\n";
            return false;
        }
    }

    // Get city name from code (unchanged)
    string getCityName(const string& code) {
        if (cities.find(code) != cities.end()) {
            return cities[code].name + " (" + code + ")";
        }
        return code;
    }

    // Get detailed city info (unchanged)
    void displayCityInfo(const string& code) {
        if (cities.find(code) == cities.end()) {
            cout << "City not found: " << code << "\n";
            return;
        }

        City& city = cities[code];
        cout << "\n" << string(50, '-') << "\n";
        cout << "City: " << city.name << " (" << city.code << ")\n";
        cout << string(50, '-') << "\n";
        cout << "Airport: " << city.airportName << "\n";
        cout << "Country: " << city.country << "\n";
        cout << "Timezone: " << city.timezone << "\n";
        cout << "Coordinates: " << city.latitude << ", " << city.longitude << "\n";
        cout << string(50, '-') << "\n\n";
    }

    // Dijkstra's Algorithm - Find cheapest route (unchanged)
    vector<Route> findCheapestRoute(const string& source, const string& dest) {
        return dijkstra(source, dest, true); // true = optimize by cost
    }

    // Dijkstra's Algorithm - Find fastest route (unchanged)
    vector<Route> findFastestRoute(const string& source, const string& dest) {
        return dijkstra(source, dest, false); // false = optimize by time
    }

    // BFS - Find route with minimum stops (unchanged)
    Route findMinimumStops(const string& source, const string& dest) {
        unordered_map<string, int> stops;
        unordered_map<string, string> parent;
        unordered_map<string, Flight> parentFlight;
        queue<string> q;

        q.push(source);
        stops[source] = 0;

        while (!q.empty()) {
            string current = q.front();
            q.pop();

            if (current == dest) break;

            if (adjList.find(current) == adjList.end()) continue;

            for (const Flight& flight : adjList[current]) {
                if (stops.find(flight.destination) == stops.end()) {
                    stops[flight.destination] = stops[current] + 1;
                    parent[flight.destination] = current;
                    parentFlight[flight.destination] = flight;
                    q.push(flight.destination);
                }
            }
        }

        // Reconstruct path
        Route route;
        if (stops.find(dest) == stops.end()) {
            return route; // No path found
        }

        vector<string> path;
        vector<Flight> flightPath;
        string current = dest;

        while (current != source) {
            path.push_back(current);
            flightPath.push_back(parentFlight[current]);
            current = parent[current];
        }
        path.push_back(source);

        reverse(path.begin(), path.end());
        reverse(flightPath.begin(), flightPath.end());

        route.cities = path;
        route.flights = flightPath;
        route.stops = flightPath.size()-1;

        for (const Flight& f : flightPath) {
            route.totalCost += f.cost;
            route.totalDuration += f.duration;
        }

        return route;
    }

    // Multi-objective Dijkstra's to find Pareto-Optimal (non-dominated) routes
    vector<Route> findParetoOptimalRoutes(const string& source, const string& dest) {
        // Map to store the set of non-dominated labels (Cost, Duration) found so far for each city
        unordered_map<string, vector<Label>> labels;

        // Use a priority queue guided by a heuristic (e.g., sum of cost and duration)
        priority_queue<PQElement, vector<PQElement>, greater<PQElement>> pq;

        // 1. Initialization
        Label initialLabel;
        initialLabel.cost = 0;
        initialLabel.duration = 0;
        initialLabel.parentCity = source;
        // parentFlight is intentionally empty for the source node

        labels[source].push_back(initialLabel);
        pq.push(PQElement(source, 0, 0));

        // 2. Main Search Loop (Labeling Algorithm)
        while (!pq.empty()) {
            PQElement currentPQ = pq.top();
            pq.pop();
            string currentCity = currentPQ.city;

            if (adjList.find(currentCity) == adjList.end()) continue;

            // Iterate over all labels found for the current city
            for (const Label& currentLabel : labels[currentCity]) {
                // IMPORTANT: We only process the label if its cost/duration matches the one we extracted
                // from the PQ (this handles stale entries, though not perfectly in a multi-label map)
                if (abs(currentLabel.cost - currentPQ.cost) > 0.001 ||
                    abs(currentLabel.duration - currentPQ.duration) > 0.001) {
                    // This label may be a duplicate or dominated, skip for simplicity.
                    // This is a simplification; a full correctness check requires a more complex PQ setup.
                    continue;
                }

                // 3. Relaxation and Dominance Check
                for (const Flight& flight : adjList[currentCity]) {
                    string nextCity = flight.destination;

                    Label newLabel;
                    newLabel.cost = currentLabel.cost + flight.cost;
                    newLabel.duration = currentLabel.duration + flight.duration;
                    newLabel.parentCity = currentCity;
                    newLabel.parentFlight = flight;

                    bool isDominated = false;
                    auto& nextLabels = labels[nextCity];

                    // Check if newLabel is dominated by an existing label in the destination city
                    for (const Label& existingLabel : nextLabels) {
                        if (newLabel.isDominatedBy(existingLabel)) {
                            isDominated = true;
                            break;
                        }
                    }

                    if (isDominated) continue; // Skip dominated path

                    // Check if newLabel dominates any existing labels
                    // We must use a separate loop to safely remove dominated labels
                    vector<Label> dominatedLabels;
                    for (const Label& existingLabel : nextLabels) {
                        if (newLabel.dominates(existingLabel)) {
                            dominatedLabels.push_back(existingLabel);
                        }
                    }

                    // Remove dominated labels
                    for (const Label& dominatedLabel : dominatedLabels) {
                        auto it = std::remove_if(nextLabels.begin(), nextLabels.end(),
                            [&](const Label& l) { return l.cost == dominatedLabel.cost && l.duration == dominatedLabel.duration; });
                        nextLabels.erase(it, nextLabels.end());
                    }

                    // Add new non-dominated label
                    bool isDuplicate = false;
                    for (const Label& existingLabel : nextLabels) {
                        if (existingLabel.cost == newLabel.cost && existingLabel.duration == newLabel.duration) {
                            isDuplicate = true;
                            break;
                        }
                    }

                    if (!isDuplicate) {
                        nextLabels.push_back(newLabel);
                        pq.push(PQElement(nextCity, newLabel.cost, newLabel.duration));
                    }
                }
            }
        }

        // 4. Reconstruct all Pareto-Optimal Routes to Destination
        vector<Route> optimalRoutes;
        if (labels.find(dest) == labels.end()) {
            return optimalRoutes; // No path found
        }

        for (const Label& finalLabel : labels[dest]) {
            Route route;
            route.totalCost = finalLabel.cost;
            route.totalDuration = finalLabel.duration;

            string currentCity = dest;
            Label currentLabel = finalLabel;

            // Reconstruct path backwards from the final label
            vector<string> path;
            vector<Flight> flightPath;

            // Loop until we reach the source
            while (currentCity != source) {
                path.push_back(currentCity);

                // Add the flight that arrived at currentCity
                flightPath.push_back(currentLabel.parentFlight);

                // Find the previous city
                string parentCityCode = currentLabel.parentCity;

                // If we are at the source, stop
                if (parentCityCode == source) break;

                // Calculate the parent label's cost/duration
                double parentCost = currentLabel.cost - currentLabel.parentFlight.cost;
                double parentDuration = currentLabel.duration - currentLabel.parentFlight.duration;

                // Search the parent city's labels for the one that matches
                bool foundParent = false;
                if (labels.count(parentCityCode)) {
                    for (const Label& parentLabel : labels[parentCityCode]) {
                        // Check for near-exact match (accounting for floating point errors)
                        if (abs(parentLabel.cost - parentCost) < 0.001 && abs(parentLabel.duration - parentDuration) < 0.001) {
                            currentCity = parentCityCode;
                            currentLabel = parentLabel;
                            foundParent = true;
                            break;
                        }
                    }
                }

                if (!foundParent) {
                    // Fallback for safety - should not happen if logic is perfect
                    path.clear();
                    flightPath.clear();
                    break;
                }
            }

            if (!path.empty()) {
                path.push_back(source);
                reverse(path.begin(), path.end());
                reverse(flightPath.begin(), flightPath.end());

                route.cities = path;
                route.flights = flightPath;
                route.stops = flightPath.size(); // stops is flight count - 1 if layovers, but since it's just path length...
                route.stops = flightPath.empty() ? 0 : flightPath.size() - 1;

                optimalRoutes.push_back(route);
            }
        }

        // Sort the optimal routes by cost for clean display
        sort(optimalRoutes.begin(), optimalRoutes.end(), [](const Route& a, const Route& b) {
            if (a.totalCost != b.totalCost) return a.totalCost < b.totalCost;
            return a.totalDuration < b.totalDuration;
            });

        return optimalRoutes;
    }

    void displayGraph() const {
        cout << "\n--- ENTIRE FLIGHT GRAPH (ADJACENCY LIST) ---\n";
        cout << "Format: SOURCE -> [Flight_Number] DESTINATION (Duration, Cost, Departure, Arrival)\n\n";

        // Collect all source city codes (keys in adjList)
        vector<string> sortedCities;
        for (const auto& pair : adjList) {
            sortedCities.push_back(pair.first);
        }

        // Sort the keys (source cities) for clean, reproducible output
        sort(sortedCities.begin(), sortedCities.end());

        for (const string& sourceCity : sortedCities) {
            // Retrieve the list of outbound flights
            const vector<Flight>& outboundFlights = adjList.at(sourceCity);

            // Print the source city header
            cout << "\n" << sourceCity << " (" << outboundFlights.size() << " outbound flights):\n";

            // Print all outbound flights from this city
            for (const Flight& flight : outboundFlights) {
                cout << "  - ["
                    << flight.flightNo << "] " // Using flightNo
                    << flight.destination
                    << " (Air Time: " << fixed << setprecision(1) << flight.duration << "h, " // Using duration
                    << "Cost: $" << fixed << setprecision(0) << flight.cost << ", " // Using cost
                    << "Dep: " << flight.departureTime << ", Arr: " << flight.arrivalTime << ")\n"; // Using departureTime/arrivalTime
            }
        }
        cout << "\n--------------------------------------------\n";
    }

    // Display route beautifully
    void displayRoute(const Route& route, const string& label) {
        if (route.cities.empty()) {
            cout << "\nNo route found!\n\n";
            return;
        }

        cout << "\n" << string(70, '-') << "\n";
        cout << "  " << label << "\n";
        cout << string(70, '-') << "\n";

        cout << "Total Cost: $" << fixed << setprecision(2) << route.totalCost << "\n";
        cout << "Total Duration: " << route.totalDuration << " hours";

        if (route.totalDuration >= 24) {
            cout << " (" << (int)(route.totalDuration / 24) << "d "
                << (int)((int)route.totalDuration % 24) << "h)";
        }
        cout << "\n";

        cout << "Number of Stops: " << route.stops << "\n";
        cout << string(70, '-') << "\n\n";

        for (size_t i = 0; i < route.flights.size(); i++) {
            const Flight& f = route.flights[i];

            cout << "Flight " << (i + 1) << ": " << f.flightNo << "\n";
            cout << "   " << getCityName(route.cities[i]) << " -> "
                << getCityName(f.destination) << "\n";
            cout << "   Airline: " << f.airline << "\n";

            if (!f.departureTime.empty()) {
                cout << "   Departure: " << f.departureTime
                    << " | Arrival: " << f.arrivalTime << "\n";
            }

            cout << "   Duration: " << f.duration << "h | Cost: $"
                << fixed << setprecision(2) << f.cost << "\n";

            if (!f.aircraft.empty()) {
                cout << "   Aircraft: " << f.aircraft;
                if (f.seatsAvailable > 0) {
                    cout << " | Seats: " << f.seatsAvailable;
                }
                cout << "\n";
            }

            if (i < route.flights.size() - 1) {
                cout << "\n   Layover at " << getCityName(f.destination) << "\n\n";
            }
        }

        cout << string(70, '-') << "\n\n";
    }

    void displayMultipleRoutes(const vector<Route>& routes, const string& title) {
        if (routes.empty()) {
            cout << "\nNo routes found for " << title << ".\n";
            return;
        }

        cout << "\n" << string(60, '=') << "\n";
        cout << " ALL OPTIMAL " << title << " ROUTES (" << routes.size() << " found)\n";
        cout << string(60, '=') << "\n";

        int counter = 1;
        for (const auto& route : routes) {
            cout << "\n--- Route " << counter++ << ": ---\n";
            cout << "   Total Cost: $" << route.totalCost << endl;
            cout << "   Total Duration: " << route.totalDuration << " hours" << endl;
            cout << "   Total Stops: " << route.stops << endl;

            // Print city path
            cout << "   Path: ";
            for (size_t i = 0; i < route.cities.size(); ++i) {
                cout << route.cities[i];
                if (i < route.cities.size() - 1) {
                    cout << " -> ";
                }
            }
            cout << endl;
        }
    }

    //Display multiple routes
    void displayParetoRoutes(const vector<Route>& routes) {
        if (routes.empty()) {
            cout << "\nNo Pareto-Optimal routes found!\n\n";
            return;
        }

        cout << "\n" << string(70, '=') << "\n";
        cout << " PARETO-OPTIMAL ROUTE OPTIONS (Non-Dominated)\n";
        cout << " (Best compromises between Cost and Duration)\n";
        cout << string(70, '=') << "\n";

        // Display summary table
        cout << left << setw(8) << "OPTION"
            << setw(15) << "TOTAL COST"
            << setw(20) << "TOTAL DURATION"
            << setw(10) << "STOPS" << "\n";
        cout << string(70, '-') << "\n";

        int option = 1;
        for (const auto& route : routes) {
            cout << left << setw(8) << to_string(option) + "."
                << "$" << setw(14) << fixed << setprecision(2) << route.totalCost
                << setw(17) << to_string(route.totalDuration) + " hours"
                << setw(10) << route.stops << "\n";
            option++;
        }
        cout << string(70, '=') << "\n";

        cout << "\nEnter option number for full details, or 0 to return to menu: ";
        int choice;
        cin >> choice;

        if (choice > 0 && choice <= routes.size()) {
            displayRoute(routes[choice - 1], "PARETO OPTIMAL ROUTE (Option " + to_string(choice) + ")");
        }
        else if (choice != 0) {
            cout << "Invalid option.\n";
        }
    }

    // Display graph statistics (unchanged)
    void displayStats() {
        cout << "\nNETWORK STATISTICS\n";
        cout << string(40, '-') << "\n";
        cout << "Total Cities: " << cities.size() << "\n";

        int totalFlights = 0;
        for (const auto& pair : adjList) {
            totalFlights += pair.second.size();
        }
        cout << "Total Flights: " << totalFlights << "\n";
        cout << "Average Routes per City: "
            << (adjList.empty() ? 0 : totalFlights / adjList.size()) << "\n";

        // Find hub cities (most connections)
        vector<pair<string, int>> cityConnections;
        for (const auto& pair : adjList) {
            cityConnections.push_back({ pair.first, pair.second.size() });
        }
        sort(cityConnections.begin(), cityConnections.end(),
            [](const pair<string, int>& a, const pair<string, int>& b) {
                return a.second > b.second;
            });

        cout << "\nTop Hub Cities:\n";
        for (size_t i = 0; i < min(size_t(5), cityConnections.size()); i++) {
            cout << "   " << (i + 1) << ". "
                << getCityName(cityConnections[i].first)
                << " - " << cityConnections[i].second << " outbound flights\n";
        }
        cout << "\n";
    }

    // List available cities (unchanged)
    void listCities() {
        cout << "\nAVAILABLE CITIES\n";
        cout << string(70, '-') << "\n";

        vector<pair<string, string>> cityList;
        for (const auto& pair : cities) {
            cityList.push_back({ pair.first, pair.second.name });
        }
        sort(cityList.begin(), cityList.end());

        for (const auto& city : cityList) {
            cout << left << setw(6) << city.first << " - " << city.second << "\n";
        }
        cout << "\nTotal: " << cityList.size() << " cities\n\n";
    }

private:
    // Generic Dijkstra implementation
    vector<Route> dijkstra(const string& source, const string& dest, bool optimizeByCost) {

        // Store the best primary metric distance (cost or duration depending on optimizeByCost)
        unordered_map<string, double> distance;
        // Store the secondary metric for tiebreaking
        unordered_map<string, double> secondaryDistance;

        // Store multiple optimal parents: city -> list of (parent_city, flight_used)
        unordered_map<string, vector<pair<string, Flight>>> parentCandidates;

        priority_queue<PQNode, vector<PQNode>, greater<PQNode>> pq;

        // Initialize cities with outbound flights
        for (const auto& pair : adjList) {
            distance[pair.first] = INF;
            secondaryDistance[pair.first] = INF;

            // Also initialize all destinations reachable from this city
            for (const Flight& flight : pair.second) {
                if (distance.find(flight.destination) == distance.end()) {
                    distance[flight.destination] = INF;
                    secondaryDistance[flight.destination] = INF;
                }
            }
        }

        // Ensure source and destination are initialized 
        if (distance.find(source) == distance.end()) {
            distance[source] = INF;
            secondaryDistance[source] = INF;
        }
        if (distance.find(dest) == distance.end()) {
            distance[dest] = INF;
            secondaryDistance[dest] = INF;
        }

        // Start from source
        distance[source] = 0;
        secondaryDistance[source] = 0;

        pq.push({ source, 0, 0 });

        while (!pq.empty()) {
            PQNode current = pq.top();
            pq.pop();

            string currentCity = current.city;
            double currentPrimaryDist = current.cost;    // Primary metric from PQ
            double currentSecondaryDist = current.duration; // Secondary metric from PQ

            if (currentPrimaryDist > distance[currentCity] + EPSILON) {
                continue;
            }
            // Also skip if primary is equal but secondary is worse
            if (abs(currentPrimaryDist - distance[currentCity]) < EPSILON &&
                currentSecondaryDist > secondaryDistance[currentCity] + EPSILON) {
                continue;
            }

            // Check if current city has outbound flights
            if (adjList.find(currentCity) == adjList.end()) {
                continue; // Dead-end city, no outbound flights
            }

            // Relax all edges from current city
            for (const Flight& flight : adjList[currentCity]) {
                string nextCity = flight.destination;

                // Define primary and secondary metrics based on optimization goal
                double primaryWeight = optimizeByCost ? flight.cost : flight.duration;
                double secondaryWeight = optimizeByCost ? flight.duration : flight.cost;

                double newPrimaryDist = distance[currentCity] + primaryWeight;
                double newSecondaryDist = secondaryDistance[currentCity] + secondaryWeight;

                bool replace = false; // New path strictly better
                bool append = false;  // New path equally good (alternative route)

                // === FIX 4: Robust comparison with epsilon for floating point ===
                if (newPrimaryDist < distance[nextCity] - EPSILON) {
                    // Strictly better primary metric (e.g., cheaper or faster)
                    replace = true;
                }
                else if (abs(newPrimaryDist - distance[nextCity]) < EPSILON) {
                    // Equal primary metric, check secondary
                    if (newSecondaryDist < secondaryDistance[nextCity] - EPSILON) {
                        // Better secondary metric (e.g., same cost but faster, or same time but cheaper)
                        replace = true;
                    }
                    else if (abs(newSecondaryDist - secondaryDistance[nextCity]) < EPSILON) {
                        // Equal on BOTH metrics - this is an alternative path with same cost AND time
                        append = true;
                    }
                }

                if (replace || append) {
                    if (replace) {
                        // Update best distances
                        distance[nextCity] = newPrimaryDist;
                        secondaryDistance[nextCity] = newSecondaryDist;
                        // Clear old parent candidates (they're now dominated)
                        parentCandidates[nextCity].clear();
                        // Push to priority queue with both metrics
                        pq.push({ nextCity, newPrimaryDist, newSecondaryDist });
                    }

                    // Add this parent as a candidate (for both replace and append cases)
                    parentCandidates[nextCity].push_back({ currentCity, flight });
                }
            }
        }

        //path reconstruction check
        vector<Route> finalRoutes;

        // Check if destination was reached
        if (distance.find(dest) != distance.end() && distance[dest] < INF - EPSILON) {
            // Use the recursive helper to find ALL optimal paths
            reconstructAllPaths(dest, source, parentCandidates, finalRoutes, Route());
        }

        return finalRoutes;
    }
};



int main() {
    FlightGraph graph;

    cout << "\n";
    cout << "--------------------------------------------------\n";
    cout << "           SMART AIRLINE ROUTE FINDER             \n";
    cout << "--------------------------------------------------\n\n";

    // Load cities from separate file
    if (!graph.loadCitiesFromJSON("cities.json")) {
        cerr << "\nFailed to load cities data!\n";
        cerr << "Please ensure 'cities.json' exists.\n\n";
        return 1;
    }

    // Load flights from separate file
    if (!graph.loadFlightsFromJSON("flights.json")) {
        cerr << "\nFailed to load flights data!\n";
        cerr << "Please ensure 'flights.json' exists.\n\n";
        return 1;
    }

    graph.displayStats();

    int choice;
    string source, dest;

    while (true) {
        displayMenu();
        cin >> choice;

        if (choice == 0) {
            cout << "\nThank you for using Smart Airline Route Finder!\n";
            cout << "Safe travels!\n\n";
            break;
        }

        if (choice >= 1 && choice <= 5) {
            cout << "\nEnter source city code (e.g., KHI, ISB, LHE): ";
            cin >> source;
            cout << "Enter destination city code (e.g., LHR, DXB, JFK): ";
            cin >> dest;

            // Convert to uppercase
            transform(source.begin(), source.end(), source.begin(), ::toupper);
            transform(dest.begin(), dest.end(), dest.begin(), ::toupper);

            cout << "\nSearching for routes from " << source << " to " << dest << "...\n";
        }

        switch (choice) {
        case 1: {
            vector<Route> cheapest = graph.findCheapestRoute(source, dest);
            graph.displayMultipleRoutes(cheapest, "CHEAPEST");
            break;
        }
        case 2: {
            vector<Route> fastest = graph.findFastestRoute(source, dest);
            graph.displayMultipleRoutes(fastest, "FASTEST");
            break;
        }
        case 3: {
            Route minStops = graph.findMinimumStops(source, dest);
            graph.displayRoute(minStops, "MINIMUM STOPS ROUTE (BFS)");
            break;
        }
        case 4: { 
            vector<Route> paretoRoutes = graph.findParetoOptimalRoutes(source, dest);
            graph.displayParetoRoutes(paretoRoutes);
            break;
        }
        case 5: {
            cout << "\nFinding all optimal routes...\n";
            vector<Route> cheapest = graph.findCheapestRoute(source, dest);
            vector<Route> fastest = graph.findFastestRoute(source, dest);
            Route minStops = graph.findMinimumStops(source, dest);

            // Display all optimal paths
            graph.displayMultipleRoutes(cheapest, "CHEAPEST");
            graph.displayMultipleRoutes(fastest, "FASTEST");
            graph.displayRoute(minStops, "Option 3: MINIMUM STOPS (BFS)");

            cout << "\nRecommendation: ";

            if (cheapest.empty() || fastest.empty()) {
                cout << "Could not find all required routes for comparison.\n\n";
            }
            else {
                // Access the single best route (the 0th element) for comparison.
                // Since all routes in the vector are equally optimal on the primary metric, 
                // using the first element is safe.
                const Route& cheapestRoute = cheapest[0];
                const Route& fastestRoute = fastest[0];

                if (cheapestRoute.totalCost < fastestRoute.totalCost * 0.7) {
                    cout << "Choose Option 1 (Best value for money: $" << cheapestRoute.totalCost << ")\n\n";
                }
                else if (fastestRoute.totalDuration < cheapestRoute.totalDuration * 0.7) {
                    cout << "Choose Option 2 (Saves significant time: " << fastestRoute.totalDuration << " hours)\n\n";
                }
                else {
                    // The routes are balanced, suggesting the user should look at the trade-offs manually
                    cout << "The routes are relatively balanced. Consider Option 3 (Minimum Stops) or Option 5 (Pareto Optimal) for a trade-off decision.\n\n";
                }
            }

            break;
        }
        case 6: {
            graph.displayStats();
            break;
        }
        case 7: {
            graph.listCities();
            break;
        }
        case 8: {
            string cityCode;
            cout << "\nEnter city code: ";
            cin >> cityCode;
            transform(cityCode.begin(), cityCode.end(), cityCode.begin(), ::toupper);
            graph.displayCityInfo(cityCode);
            break;
        }
        case 9: { 
            graph.displayGraph();
            break;
        }
        default:
            cout << "\nInvalid choice! Please try again.\n";
        }

        if (choice >= 1 && choice <= 9) {
            cout << "Press Enter to continue...";
            // Clear cin buffer
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cin.get();
        }
    }

    return 0;
}