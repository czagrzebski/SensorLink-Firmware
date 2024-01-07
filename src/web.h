#ifndef WEB_H
#define WEB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* html_template = "SensorLink - Sensor Data /* Styles remain the same as before */ body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; } nav { background-color: #333; color: white; padding: 10px 30px; /* Adjusted navbar height */ display: flex; justify-content: space-between; align-items: center; } nav h1 { margin: 0; /* Resetting margin */ } nav ul { list-style: none; margin: 0; padding: 0; display: flex; align-items: center; /* Align items vertically */ } nav ul li { margin-right: 20px; } nav ul li a { color: white; text-decoration: none; } #wifi-info { font-size: 14px; display: flex; align-items: center; color: #ccc; /* Color for WiFi info */ margin-left: auto; /* Pushes WiFi info to the far right */ } #wifi-icon { margin-right: 5px; } #ssid { margin-right: 10px; /* Add space between SSID and time */ } #current-time { } .card-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); /* Grid layout for cards */ gap: 20px; padding: 20px; max-width: 1200px; /* Max width for the grid */ margin: 0 auto; } .card { background-color: white; border-radius: 5px; padding: 20px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); } .card h1 { margin-top: 0; } .card p { margin-bottom: 5px; } SensorLink ESP32Link Sensor 1 (Potentiometer) Value: -- -- mV Sensor 2 (Not Detected) Failed to read sensor data function updateTime() { const now = new Date(); const formattedTime = now.toLocaleTimeString(); document.getElementById('current-time').textContent = formattedTime; } updateTime(); setInterval(updateTime, 1000);";

char *generate_homepage();

#endif