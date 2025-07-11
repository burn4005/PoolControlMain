// Pool Controller Mobile App JavaScript
class PoolControllerApp {
    constructor() {
        this.ws = null;
        this.reconnectInterval = null;
        this.currentScreen = 'dashboard';
        this.systemData = {};
        this.isConnected = false;
        
        this.init();
    }

    init() {
        this.setupEventListeners();
        this.connectWebSocket();
        this.loadInitialData();
        this.startClock();
        this.hideLoadingOverlay();
    }

    setupEventListeners() {
        // Navigation
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const screen = e.currentTarget.dataset.screen;
                this.switchScreen(screen);
            });
        });

        // Light control buttons
        document.querySelectorAll('.light-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const mode = parseInt(e.currentTarget.dataset.mode);
                this.setLightMode(mode);
            });
        });

        // Quick action buttons
        document.getElementById('emergencyStop')?.addEventListener('click', () => {
            this.emergencyStop();
        });

        document.getElementById('quickDose')?.addEventListener('click', () => {
            this.switchScreen('manual');
        });

        document.getElementById('lightToggle')?.addEventListener('click', () => {
            this.toggleLights();
        });

        // Manual control buttons
        document.getElementById('manualDoseBtn')?.addEventListener('click', () => {
            this.manualDose();
        });

        document.getElementById('refillBtn')?.addEventListener('click', () => {
            this.refillAcidBottle();
        });

        // Settings buttons
        document.getElementById('saveSettings')?.addEventListener('click', () => {
            this.saveSettings();
        });

        document.getElementById('resetSettings')?.addEventListener('click', () => {
            this.resetSettings();
        });

        // Pump calibration buttons
        document.getElementById('startCalibrationBtn')?.addEventListener('click', () => {
            this.startPumpCalibration();
        });

        document.getElementById('completeCalibrationBtn')?.addEventListener('click', () => {
            this.completePumpCalibration();
        });

        document.getElementById('clearCalibrationBtn')?.addEventListener('click', () => {
            this.clearPumpCalibration();
        });

        document.getElementById('pumpStatusBtn')?.addEventListener('click', () => {
            this.getPumpStatus();
        });

        // Sensor calibration buttons
        document.getElementById('phCal4Btn')?.addEventListener('click', () => {
            this.calibratePh(4.0);
        });

        document.getElementById('phCal7Btn')?.addEventListener('click', () => {
            this.calibratePh(7.0);
        });

        document.getElementById('phCal10Btn')?.addEventListener('click', () => {
            this.calibratePh(10.0);
        });

        document.getElementById('phCalCustomBtn')?.addEventListener('click', () => {
            this.calibratePhCustom();
        });

        document.getElementById('clearPhCalBtn')?.addEventListener('click', () => {
            this.clearPhCalibration();
        });

        document.getElementById('orpCal225Btn')?.addEventListener('click', () => {
            this.calibrateOrp(225);
        });

        document.getElementById('orpCal470Btn')?.addEventListener('click', () => {
            this.calibrateOrp(470);
        });

        document.getElementById('orpCalCustomBtn')?.addEventListener('click', () => {
            this.calibrateOrpCustom();
        });

        document.getElementById('clearOrpCalBtn')?.addEventListener('click', () => {
            this.clearOrpCalibration();
        });

        // Pull to refresh
        let startY = 0;
        let pullDistance = 0;
        const pullThreshold = 100;

        document.addEventListener('touchstart', (e) => {
            if (window.scrollY === 0) {
                startY = e.touches[0].clientY;
            }
        });

        document.addEventListener('touchmove', (e) => {
            if (startY > 0) {
                pullDistance = e.touches[0].clientY - startY;
                if (pullDistance > 0 && pullDistance < pullThreshold) {
                    e.preventDefault();
                }
            }
        });

        document.addEventListener('touchend', () => {
            if (pullDistance > pullThreshold) {
                this.refreshData();
            }
            startY = 0;
            pullDistance = 0;
        });

        // Prevent zoom on double tap
        let lastTouchEnd = 0;
        document.addEventListener('touchend', (e) => {
            const now = (new Date()).getTime();
            if (now - lastTouchEnd <= 300) {
                e.preventDefault();
            }
            lastTouchEnd = now;
        }, false);
    }

    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        
        try {
            this.ws = new WebSocket(wsUrl);
            
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.isConnected = true;
                this.updateConnectionStatus(true);
                if (this.reconnectInterval) {
                    clearInterval(this.reconnectInterval);
                    this.reconnectInterval = null;
                }
            };
            
            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.updateSystemData(data);
                } catch (error) {
                    console.error('Error parsing WebSocket message:', error);
                }
            };
            
            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.isConnected = false;
                this.updateConnectionStatus(false);
                this.scheduleReconnect();
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.isConnected = false;
                this.updateConnectionStatus(false);
            };
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.scheduleReconnect();
        }
    }

    scheduleReconnect() {
        if (!this.reconnectInterval) {
            this.reconnectInterval = setInterval(() => {
                console.log('Attempting to reconnect...');
                this.connectWebSocket();
            }, 5000);
        }
    }

    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connectionStatus');
        const statusDot = statusElement?.querySelector('.status-dot');
        const statusText = statusElement?.querySelector('.status-text');
        
        if (statusDot && statusText) {
            if (connected) {
                statusDot.style.backgroundColor = '#00AA00';
                statusText.textContent = 'Connected';
            } else {
                statusDot.style.backgroundColor = '#FF0000';
                statusText.textContent = 'Disconnected';
            }
        }
    }

    async loadInitialData() {
        try {
            const response = await fetch('/api/status');
            if (response.ok) {
                const data = await response.json();
                this.updateSystemData(data);
            }
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.showToast('Failed to load system data', 'error');
        }
    }

    updateSystemData(data) {
        this.systemData = { ...this.systemData, ...data };
        this.updateUI();
    }

    updateUI() {
        this.updateDashboard();
        this.updateLightingScreen();
        this.updateManualScreen();
        this.updateSettingsScreen();
        this.updateAlarmsScreen();
        this.updateHeader();
    }

    updateHeader() {
        // Update time
        const timeElement = document.getElementById('currentTime');
        if (timeElement) {
            const now = new Date();
            timeElement.textContent = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        }

        // Update acid level in header
        const acidLevelElement = document.getElementById('acidLevel');
        const acidPercentageElement = acidLevelElement?.querySelector('.acid-percentage');
        if (acidPercentageElement && this.systemData.chemical) {
            const percentage = Math.round((this.systemData.chemical.acid_remaining_ml / 5000) * 100);
            acidPercentageElement.textContent = `${percentage}%`;
        }

        // Update network status
        const networkStatusElement = document.getElementById('networkStatus');
        const networkTextElement = networkStatusElement?.querySelector('.network-text');
        if (networkTextElement && this.systemData.network) {
            networkTextElement.textContent = this.systemData.network.status;
            networkTextElement.className = `network-text ${this.systemData.network.status}`;
        }
    }

    updateDashboard() {
        if (this.currentScreen !== 'dashboard') return;

        const { sensors, equipment, chemical, learning, alarms } = this.systemData;

        // Update sensor values
        if (sensors) {
            this.updateElement('temperature', `${sensors.temperature?.toFixed(1) || '--'}°C`);
            this.updateElement('ph', sensors.ph?.toFixed(2) || '--');
            this.updateElement('orp', `${Math.round(sensors.orp) || '--'}mV`);
            
            // Update sensor health
            const healthElement = document.getElementById('sensorHealth');
            const healthIndicator = healthElement?.querySelector('.health-indicator');
            const healthText = healthElement?.querySelector('span:last-child');
            
            if (healthIndicator && healthText) {
                if (sensors.healthy) {
                    healthIndicator.className = 'health-indicator healthy';
                    healthText.textContent = 'Sensors Healthy';
                } else {
                    healthIndicator.className = 'health-indicator warning';
                    healthText.textContent = 'Sensors Stabilizing';
                }
            }
        }

        // Update equipment status
        if (equipment) {
            const pumpStatusMap = ['OFF', 'LOW SPEED', 'MEDIUM SPEED', 'HIGH SPEED', 'ON BUT STOPPED'];
            this.updateElement('pumpStatus', pumpStatusMap[equipment.pump_status] || 'UNKNOWN');
            this.updateElement('pumpCurrent', `${equipment.pump_current?.toFixed(2) || '--'}A`);
            
            if (equipment.chlorinator_relay_on) {
                this.updateElement('chlorinatorStatus', `${Math.round(this.systemData.config?.chlorinator_duty_cycle || 0)}% DUTY`);
            } else {
                this.updateElement('chlorinatorStatus', 'OFF');
            }
            this.updateElement('chlorinatorCurrent', `${equipment.chlorinator_current?.toFixed(2) || '--'}A`);
            
            const lightModes = ['Off', 'Blue', 'Pink', 'Red', 'Yellow', 'Green', 'Cyan', 'White', 'Mode 1', 'Mode 2', 'Mode 3', 'Mode 4', 'Brightness'];
            this.updateElement('lightStatus', lightModes[equipment.current_light_mode] || 'Unknown');
            
            // Update status indicators
            this.updateStatusIndicator('pumpIndicator', equipment.pump_healthy);
            this.updateStatusIndicator('chlorinatorIndicator', !alarms?.chlorinator_alarm);
            this.updateStatusIndicator('lightIndicator', equipment.light_relay_on);
        }

        // Update chemical system
        if (chemical) {
            this.updateElement('acidAmount', `${(chemical.acid_remaining_ml / 1000).toFixed(1)}L / 5.0L`);
            this.updateElement('dailyConsumption', `Daily: ${Math.round(chemical.daily_consumption)}ml`);
            this.updateElement('daysRemaining', `${chemical.days_remaining} days left`);
            
            // Update acid bar
            const acidFill = document.getElementById('acidFill');
            if (acidFill) {
                const percentage = (chemical.acid_remaining_ml / 5000) * 100;
                acidFill.style.width = `${percentage}%`;
                
                // Update color based on level
                if (percentage < 10) {
                    acidFill.style.backgroundColor = '#FF0000';
                } else if (percentage < 20) {
                    acidFill.style.backgroundColor = '#FFAA00';
                } else {
                    acidFill.style.backgroundColor = '#00AA00';
                }
            }
        }

        // Update learning system
        if (learning) {
            this.updateElement('phLearning', `${learning.ph_learning_gain?.toFixed(1) || '--'}%`);
            this.updateElement('orpLearning', `${learning.orp_learning_gain?.toFixed(1) || '--'}%`);
        }
    }

    updateLightingScreen() {
        if (this.currentScreen !== 'lighting') return;

        const { equipment } = this.systemData;
        if (!equipment) return;

        const lightModes = ['Off', 'Blue', 'Pink', 'Red', 'Yellow', 'Green', 'Cyan', 'White', 'Mode 1', 'Mode 2', 'Mode 3', 'Mode 4', 'Brightness'];
        this.updateElement('currentLightMode', lightModes[equipment.current_light_mode] || 'Unknown');

        // Update runtime (placeholder - would need actual runtime data)
        this.updateElement('lightRuntime', equipment.light_relay_on ? '1h 23m' : '--');

        // Highlight active button
        document.querySelectorAll('.light-btn').forEach(btn => {
            btn.classList.remove('active');
        });
        
        const activeBtn = document.querySelector(`[data-mode="${equipment.current_light_mode}"]`);
        if (activeBtn) {
            activeBtn.classList.add('active');
        }
    }

    updateManualScreen() {
        // Update safety notices and button states based on system status
        const manualDoseBtn = document.getElementById('manualDoseBtn');
        if (manualDoseBtn && this.systemData.equipment) {
            manualDoseBtn.disabled = !this.systemData.equipment.pump_healthy;
        }
    }

    updateSettingsScreen() {
        if (this.currentScreen !== 'settings') return;

        const { config, learning } = this.systemData;
        if (!config) return;

        // Update setting inputs
        this.updateInputValue('phTarget', config.ph_target);
        this.updateInputValue('orpTarget', Math.round(config.orp_target));
        this.updateInputValue('dutyCycle', Math.round(config.chlorinator_duty_cycle));
        this.updateInputValue('poolVolume', config.pool_volume_liters);
        this.updateInputValue('acidConcentration', config.hcl_concentration_percent);

        // Update learning effectiveness
        if (learning) {
            this.updateElement('phEffectiveness', `${Math.round(learning.ph_effectiveness || 0)}%`);
            this.updateElement('orpEffectiveness', `${Math.round(learning.orp_effectiveness || 0)}%`);
        }
    }

    updateAlarmsScreen() {
        if (this.currentScreen !== 'alarms') return;

        const { alarms, sensors, equipment } = this.systemData;
        
        // Update alarm list
        const alarmList = document.getElementById('alarmList');
        if (alarmList) {
            const activeAlarms = [];
            
            if (alarms?.pump_alarm) activeAlarms.push('Pump alarm active');
            if (alarms?.chlorinator_alarm) activeAlarms.push('Chlorinator alarm active');
            if (alarms?.acid_low_alarm) activeAlarms.push('Acid level low');
            
            if (activeAlarms.length === 0) {
                alarmList.innerHTML = '<div class="no-alarms">✅ No active alarms</div>';
            } else {
                alarmList.innerHTML = activeAlarms.map(alarm => 
                    `<div class="alarm-item">${alarm}</div>`
                ).join('');
            }
        }

        // Update system health
        this.updateHealthStatus('sensorsHealth', sensors?.healthy);
        this.updateHealthStatus('pumpHealth', equipment?.pump_healthy);
        this.updateHealthStatus('chlorinatorHealth', !alarms?.chlorinator_alarm);
        this.updateHealthStatus('chemicalHealth', !alarms?.acid_low_alarm);
    }

    updateElement(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }

    updateInputValue(id, value) {
        const input = document.getElementById(id);
        if (input && value !== undefined) {
            input.value = value;
        }
    }

    updateStatusIndicator(id, healthy) {
        const indicator = document.getElementById(id);
        if (indicator) {
            indicator.style.backgroundColor = healthy ? '#00AA00' : '#FF0000';
        }
    }

    updateHealthStatus(id, healthy) {
        const element = document.getElementById(id);
        if (element) {
            element.className = `health-status ${healthy ? 'healthy' : 'danger'}`;
            element.textContent = healthy ? 'Healthy' : 'Warning';
        }
    }

    switchScreen(screenName) {
        // Hide all screens
        document.querySelectorAll('.screen').forEach(screen => {
            screen.classList.remove('active');
        });

        // Show target screen
        const targetScreen = document.getElementById(`${screenName}Screen`);
        if (targetScreen) {
            targetScreen.classList.add('active');
            this.currentScreen = screenName;
        }

        // Update navigation
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.classList.remove('active');
        });
        
        const activeNavBtn = document.querySelector(`[data-screen="${screenName}"]`);
        if (activeNavBtn) {
            activeNavBtn.classList.add('active');
        }

        // Update screen-specific data
        this.updateUI();
    }

    async setLightMode(mode) {
        try {
            const response = await fetch('/api/lighting', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ mode })
            });

            if (response.ok) {
                const lightModes = ['Off', 'Blue', 'Pink', 'Red', 'Yellow', 'Green', 'Cyan', 'White', 'Mode 1', 'Mode 2', 'Mode 3', 'Mode 4', 'Brightness'];
                this.showToast(`Light mode set to ${lightModes[mode]}`);
            } else {
                throw new Error('Failed to set light mode');
            }
        } catch (error) {
            console.error('Error setting light mode:', error);
            this.showToast('Failed to set light mode', 'error');
        }
    }

    async manualDose() {
        const doseAmount = document.getElementById('doseAmount')?.value;
        if (!doseAmount || doseAmount <= 0) {
            this.showToast('Please enter a valid dose amount', 'error');
            return;
        }

        if (!this.systemData.equipment?.pump_healthy) {
            this.showToast('Cannot dose - pump not healthy', 'error');
            return;
        }

        try {
            const response = await fetch('/api/dose_acid', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ volume: parseFloat(doseAmount) })
            });

            if (response.ok) {
                this.showToast(`Dosed ${doseAmount}ml of acid`);
            } else {
                throw new Error('Failed to dose acid');
            }
        } catch (error) {
            console.error('Error dosing acid:', error);
            this.showToast('Failed to dose acid', 'error');
        }
    }

    async saveSettings() {
        const settings = {
            ph_target: parseFloat(document.getElementById('phTarget')?.value),
            chlorinator_duty_cycle: parseFloat(document.getElementById('dutyCycle')?.value)
        };

        try {
            const response = await fetch('/api/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settings)
            });

            if (response.ok) {
                this.showToast('Settings saved successfully');
            } else {
                throw new Error('Failed to save settings');
            }
        } catch (error) {
            console.error('Error saving settings:', error);
            this.showToast('Failed to save settings', 'error');
        }
    }

    async resetSettings() {
        if (confirm('Reset all settings to defaults?')) {
            try {
                const response = await fetch('/api/reset_settings', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    }
                });

                if (response.ok) {
                    const result = await response.json();
                    this.showToast(result.message || 'Settings reset to defaults');
                    // Reload data to show updated settings
                    await this.loadInitialData();
                } else {
                    throw new Error('Failed to reset settings');
                }
            } catch (error) {
                console.error('Error resetting settings:', error);
                this.showToast('Failed to reset settings', 'error');
            }
        }
    }

    async emergencyStop() {
        if (confirm('Emergency stop all equipment?')) {
            try {
                const response = await fetch('/api/emergency_stop', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    }
                });

                if (response.ok) {
                    const result = await response.json();
                    this.showToast(result.message || 'Emergency stop activated', 'warning');
                } else {
                    throw new Error('Failed to activate emergency stop');
                }
            } catch (error) {
                console.error('Error activating emergency stop:', error);
                this.showToast('Failed to activate emergency stop', 'error');
            }
        }
    }

    toggleLights() {
        const currentMode = this.systemData.equipment?.current_light_mode || 0;
        const newMode = currentMode === 0 ? 7 : 0; // Toggle between off and white
        this.setLightMode(newMode);
    }

    async refillAcidBottle() {
        const newVolume = prompt('Enter new acid volume (ml):', '5000');
        if (newVolume && !isNaN(newVolume)) {
            try {
                const response = await fetch('/api/refill_acid', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ volume: parseFloat(newVolume) })
                });

                if (response.ok) {
                    const result = await response.json();
                    this.showToast(result.message || `Acid bottle refilled to ${newVolume}ml`);
                    // Reload data to show updated acid level
                    await this.loadInitialData();
                } else {
                    throw new Error('Failed to refill acid bottle');
                }
            } catch (error) {
                console.error('Error refilling acid bottle:', error);
                this.showToast('Failed to refill acid bottle', 'error');
            }
        }
    }

    async startPumpCalibration() {
        const targetVolume = document.getElementById('calibrationVolume')?.value;
        if (!targetVolume || targetVolume <= 0) {
            this.showToast('Please enter a valid target volume', 'error');
            return;
        }

        try {
            const response = await fetch('/api/pump_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    action: 'start',
                    target_volume: parseFloat(targetVolume)
                })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                
                if (result.status === 'success') {
                    // Enable completion controls
                    document.getElementById('completeCalibrationBtn').disabled = false;
                    document.getElementById('actualVolume').disabled = false;
                    document.getElementById('startCalibrationBtn').disabled = true;
                    
                    // Update status
                    const statusElement = document.getElementById('calibrationStatus');
                    if (statusElement) {
                        statusElement.textContent = `Pump dispensed ${targetVolume}ml. Measure actual volume and complete calibration.`;
                        statusElement.className = 'calibration-status warning';
                    }
                }
            } else {
                throw new Error('Failed to start pump calibration');
            }
        } catch (error) {
            console.error('Error starting pump calibration:', error);
            this.showToast('Failed to start pump calibration', 'error');
        }
    }

    async completePumpCalibration() {
        const actualVolume = document.getElementById('actualVolume')?.value;
        if (!actualVolume || actualVolume <= 0) {
            this.showToast('Please enter the actual measured volume', 'error');
            return;
        }

        try {
            const response = await fetch('/api/pump_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    action: 'complete',
                    actual_volume: parseFloat(actualVolume)
                })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                
                if (result.status === 'success') {
                    // Reset calibration controls
                    document.getElementById('completeCalibrationBtn').disabled = true;
                    document.getElementById('actualVolume').disabled = true;
                    document.getElementById('actualVolume').value = '';
                    document.getElementById('startCalibrationBtn').disabled = false;
                    
                    // Update status
                    const statusElement = document.getElementById('calibrationStatus');
                    if (statusElement) {
                        statusElement.textContent = 'Pump calibration completed successfully!';
                        statusElement.className = 'calibration-status success';
                        
                        // Reset status after 5 seconds
                        setTimeout(() => {
                            statusElement.textContent = 'Ready to calibrate pump volume';
                            statusElement.className = 'calibration-status';
                        }, 5000);
                    }
                }
            } else {
                throw new Error('Failed to complete pump calibration');
            }
        } catch (error) {
            console.error('Error completing pump calibration:', error);
            this.showToast('Failed to complete pump calibration', 'error');
        }
    }

    async clearPumpCalibration() {
        if (!confirm('Clear pump calibration and restore factory defaults?')) {
            return;
        }

        try {
            const response = await fetch('/api/pump_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'clear' })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                
                // Reset calibration controls
                document.getElementById('completeCalibrationBtn').disabled = true;
                document.getElementById('actualVolume').disabled = true;
                document.getElementById('actualVolume').value = '';
                document.getElementById('startCalibrationBtn').disabled = false;
                
                // Update status
                const statusElement = document.getElementById('calibrationStatus');
                if (statusElement) {
                    statusElement.textContent = 'Pump calibration cleared - factory defaults restored';
                    statusElement.className = 'calibration-status warning';
                    
                    // Reset status after 5 seconds
                    setTimeout(() => {
                        statusElement.textContent = 'Ready to calibrate pump volume';
                        statusElement.className = 'calibration-status';
                    }, 5000);
                }
            } else {
                throw new Error('Failed to clear pump calibration');
            }
        } catch (error) {
            console.error('Error clearing pump calibration:', error);
            this.showToast('Failed to clear pump calibration', 'error');
        }
    }

    async getPumpStatus() {
        try {
            const response = await fetch('/api/pump_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'status' })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'info' : 'error');
            } else {
                throw new Error('Failed to get pump status');
            }

            // Also get total volume
            const totalResponse = await fetch('/api/pump_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'total_volume' })
            });

            if (totalResponse.ok) {
                const totalResult = await totalResponse.json();
                setTimeout(() => {
                    this.showToast(totalResult.message, totalResult.status === 'success' ? 'info' : 'error');
                }, 1000);
            }
        } catch (error) {
            console.error('Error getting pump status:', error);
            this.showToast('Failed to get pump status', 'error');
        }
    }

    async calibratePh(bufferValue) {
        try {
            const response = await fetch('/api/sensor_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    action: 'calibrate_ph',
                    buffer_value: bufferValue
                })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                this.updateSensorCalibrationStatus(`pH calibration started with buffer ${bufferValue}`);
            } else {
                throw new Error('Failed to calibrate pH');
            }
        } catch (error) {
            console.error('Error calibrating pH:', error);
            this.showToast('Failed to calibrate pH', 'error');
        }
    }

    async calibratePhCustom() {
        const bufferValue = parseFloat(document.getElementById('phBufferValue')?.value);
        if (!bufferValue || bufferValue < 1 || bufferValue > 14) {
            this.showToast('Please enter a valid pH buffer value (1.0-14.0)', 'error');
            return;
        }
        await this.calibratePh(bufferValue);
    }

    async calibrateOrp(standardMv) {
        try {
            const response = await fetch('/api/sensor_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    action: 'calibrate_orp',
                    standard_mv: standardMv
                })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                this.updateSensorCalibrationStatus(`ORP calibration started with ${standardMv}mV standard`);
            } else {
                throw new Error('Failed to calibrate ORP');
            }
        } catch (error) {
            console.error('Error calibrating ORP:', error);
            this.showToast('Failed to calibrate ORP', 'error');
        }
    }

    async calibrateOrpCustom() {
        const standardMv = parseFloat(document.getElementById('orpStandardValue')?.value);
        if (!standardMv || standardMv < -2000 || standardMv > 2000) {
            this.showToast('Please enter a valid ORP standard (-2000 to 2000mV)', 'error');
            return;
        }
        await this.calibrateOrp(standardMv);
    }

    async clearPhCalibration() {
        if (!confirm('Clear pH calibration? This will reset to factory defaults.')) {
            return;
        }

        try {
            const response = await fetch('/api/sensor_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'clear_ph_calibration' })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                this.updateSensorCalibrationStatus('pH calibration cleared');
            } else {
                throw new Error('Failed to clear pH calibration');
            }
        } catch (error) {
            console.error('Error clearing pH calibration:', error);
            this.showToast('Failed to clear pH calibration', 'error');
        }
    }

    async clearOrpCalibration() {
        if (!confirm('Clear ORP calibration? This will reset to factory defaults.')) {
            return;
        }

        try {
            const response = await fetch('/api/sensor_calibration', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ action: 'clear_orp_calibration' })
            });

            if (response.ok) {
                const result = await response.json();
                this.showToast(result.message, result.status === 'success' ? 'success' : 'error');
                this.updateSensorCalibrationStatus('ORP calibration cleared');
            } else {
                throw new Error('Failed to clear ORP calibration');
            }
        } catch (error) {
            console.error('Error clearing ORP calibration:', error);
            this.showToast('Failed to clear ORP calibration', 'error');
        }
    }

    updateSensorCalibrationStatus(message) {
        const statusElement = document.getElementById('sensorCalibrationStatus');
        if (statusElement) {
            statusElement.textContent = message;
            statusElement.className = 'calibration-status info';
            
            // Reset status after 10 seconds
            setTimeout(() => {
                statusElement.textContent = 'Ready for sensor calibration';
                statusElement.className = 'calibration-status';
            }, 10000);
        }
    }

    async refreshData() {
        this.showToast('Refreshing data...');
        await this.loadInitialData();
    }

    startClock() {
        this.updateHeader();
        setInterval(() => {
            this.updateHeader();
        }, 1000);
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        const toastMessage = document.getElementById('toastMessage');
        
        if (toast && toastMessage) {
            toastMessage.textContent = message;
            
            // Set color based on type
            switch (type) {
                case 'error':
                    toast.style.backgroundColor = '#FF0000';
                    break;
                case 'warning':
                    toast.style.backgroundColor = '#FFAA00';
                    break;
                case 'success':
                    toast.style.backgroundColor = '#00AA00';
                    break;
                default:
                    toast.style.backgroundColor = '#0066CC';
            }
            
            toast.classList.add('show');
            
            setTimeout(() => {
                toast.classList.remove('show');
            }, 3000);
        }
    }

    hideLoadingOverlay() {
        const overlay = document.getElementById('loadingOverlay');
        if (overlay) {
            setTimeout(() => {
                overlay.classList.add('hidden');
            }, 1000);
        }
    }
}

// Initialize app when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.poolApp = new PoolControllerApp();
});

// Service Worker registration for PWA
if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('/sw.js')
            .then(registration => {
                console.log('SW registered: ', registration);
            })
            .catch(registrationError => {
                console.log('SW registration failed: ', registrationError);
            });
    });
}

// Add to home screen prompt
let deferredPrompt;
window.addEventListener('beforeinstallprompt', (e) => {
    e.preventDefault();
    deferredPrompt = e;
    
    // Show install button or banner
    setTimeout(() => {
        if (deferredPrompt) {
            deferredPrompt.prompt();
            deferredPrompt.userChoice.then((choiceResult) => {
                if (choiceResult.outcome === 'accepted') {
                    console.log('User accepted the A2HS prompt');
                }
                deferredPrompt = null;
            });
        }
    }, 5000);
});
