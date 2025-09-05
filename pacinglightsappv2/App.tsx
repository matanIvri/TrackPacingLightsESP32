// App.tsx
import React, { useState, useEffect, useRef } from "react";
import {
    SafeAreaView,
    StyleSheet,
    Text,
    TouchableOpacity,
    View,
    TextInput,
    Alert,
    Platform,
    KeyboardAvoidingView,
    ScrollView,
} from "react-native";
import DeviceModal from "./DeviceConnectionModal";
import useBLE from "./useBLE";

const App = () => {
    const {
        allDevices,
        connectedDevice,
        connectToDevice,
        color,
        requestPermissions,
        scanForPeripherals,
        sendString,
    } = useBLE();

    // Open the device list immediately
    const [isModalVisible, setIsModalVisible] = useState<boolean>(true);
    const [wasConnected, setWasConnected] = useState(false);
    const prevConnectedRef = useRef<boolean>(false);

    // Wave params
    const [totalLaps, setTotalLaps] = useState<string>("");
    const [timePerLap, setTimePerLap] = useState<string>("");
    const [numLights, setNumLights] = useState<string>("");
    const [startingLight, setStartingLight] = useState<string>("");

    const isTotalLapsValid = /^\d+$/.test(totalLaps.trim());
    const isTimePerLapValid = /^\d+(\.\d+)?$/.test(timePerLap.trim());
    const isNumLightsValid = /^\d+$/.test(numLights.trim());
    const isStartingLightValid =
        /^\d+$/.test(startingLight.trim()) &&
        Number(startingLight) >= 0 &&
        Number(startingLight) <= 7;

    const isWaveParamsValid =
        isTotalLapsValid && isTimePerLapValid && isNumLightsValid && isStartingLightValid;

    // Lap time calc
    const [raceDistance, setRaceDistance] = useState<string>("");
    const [raceResult, setRaceResult] = useState<string>("");
    const [lapDistance, setLapDistance] = useState<string>("");
    const [calculatedLapTime, setCalculatedLapTime] = useState<string>("");

    // Request permissions, auto-scan, open modal on launch
    useEffect(() => {
        (async () => {
            const ok = await requestPermissions();
            if (!ok) {
                Alert.alert("Permissions required", "BLE permissions were denied.");
                return;
            }
            scanForPeripherals();
            setIsModalVisible(true);
        })();
    }, []);

    // Watch connection state
    useEffect(() => {
        const isConnected = !!connectedDevice;
        if (prevConnectedRef.current && !isConnected) {
            Alert.alert("Bluetooth disconnected", "Your device has disconnected.");
            setWasConnected(true);
            // Re-open list and rescan
            setIsModalVisible(true);
            scanForPeripherals();
        }
        prevConnectedRef.current = isConnected;
    }, [connectedDevice]);

    const handleStartWave = async () => {
        const payload = `${totalLaps.trim()},${timePerLap.trim()},${numLights.trim()},${startingLight.trim()}`;
        try {
            await sendString(payload);
            Alert.alert("Wave Started", `Sent: ${payload}`);
        } catch (e: any) {
            Alert.alert("Error", e.message ?? "Failed to start wave.");
        }
    };

    const handleStopWave = async () => {
        try {
            await sendString("0,0,0,0");
            Alert.alert("Wave Stopped", "Sent stop command");
        } catch (e: any) {
            Alert.alert("Error", e.message ?? "Failed to stop wave.");
        }
    };

    const handleCalculate = () => {
        const dist = parseFloat(raceDistance);
        const lapDist = parseFloat(lapDistance);
        const timeMatch = raceResult.trim().match(/^(\d+):(\d{2})$/);
        if (!timeMatch || isNaN(dist) || isNaN(lapDist) || dist <= 0 || lapDist <= 0) {
            Alert.alert(
                "Invalid input",
                'Ensure race distance and lap distance are positive numbers and result is min:sec (e.g. "12:34").'
            );
            return;
        }
        const minutes = parseInt(timeMatch[1], 10);
        const seconds = parseInt(timeMatch[2], 10);
        const totalSec = minutes * 60 + seconds;
        const lapSec = (totalSec / dist) * lapDist;
        setCalculatedLapTime(lapSec.toFixed(1));
    };

    return (
        <SafeAreaView style={[styles.container, { backgroundColor: color }]}>
            {/* Connected screen content */}
            {connectedDevice && (
                <KeyboardAvoidingView
                    style={{ flex: 1 }}
                    behavior={Platform.select({ ios: "padding", android: "height" })}
                    keyboardVerticalOffset={Platform.select({ ios: 0, android: 0 })}
                >
                    <ScrollView
                        contentContainerStyle={styles.scrollContent}
                        keyboardShouldPersistTaps="handled"
                    >
                        <View style={styles.topSpacer} />

                        <View style={styles.mainContent}>
                            <View style={styles.waveWrapper}>
                                <Text style={styles.sectionLabel}>Wave Parameters:</Text>

                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Total laps</Text>
                                    <TextInput
                                        style={[
                                            styles.inputField,
                                            totalLaps && !isTotalLapsValid && styles.inputError,
                                        ]}
                                        keyboardType="numeric"
                                        value={totalLaps}
                                        onChangeText={setTotalLaps}
                                    />
                                </View>
                                {totalLaps && !isTotalLapsValid && (
                                    <Text style={styles.errorText}>Enter a valid integer</Text>
                                )}

                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Time per lap (s)</Text>
                                    <TextInput
                                        style={[
                                            styles.inputField,
                                            timePerLap && !isTimePerLapValid && styles.inputError,
                                        ]}
                                        keyboardType="numeric"
                                        value={timePerLap}
                                        onChangeText={setTimePerLap}
                                    />
                                </View>
                                {timePerLap && !isTimePerLapValid && (
                                    <Text style={styles.errorText}>Enter a number</Text>
                                )}

                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Number of lights</Text>
                                    <TextInput
                                        style={[
                                            styles.inputField,
                                            numLights && !isNumLightsValid && styles.inputError,
                                        ]}
                                        keyboardType="numeric"
                                        value={numLights}
                                        onChangeText={setNumLights}
                                    />
                                </View>
                                {numLights && !isNumLightsValid && (
                                    <Text style={styles.errorText}>Enter a valid integer</Text>
                                )}

                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Starting light</Text>
                                    <TextInput
                                        style={[
                                            styles.inputField,
                                            startingLight && !isStartingLightValid && styles.inputError,
                                        ]}
                                        keyboardType="numeric"
                                        value={startingLight}
                                        onChangeText={setStartingLight}
                                        placeholder="0–7"
                                    />
                                </View>
                                <Text style={styles.helpText}>0 is the first node.</Text>
                                {startingLight && !isStartingLightValid && (
                                    <Text style={styles.errorText}>
                                        Enter an integer 0–7 (0 is the first node)
                                    </Text>
                                )}

                                {/* Start/Stop in a row */}
                                <View style={styles.buttonRow}>
                                    <TouchableOpacity
                                        onPress={handleStartWave}
                                        disabled={!isWaveParamsValid}
                                        style={[
                                            styles.rowButton,
                                            isWaveParamsValid ? styles.startButtonValid : styles.disabledButton,
                                        ]}
                                    >
                                        <Text style={styles.rowButtonText}>Start Wave</Text>
                                    </TouchableOpacity>

                                    <TouchableOpacity
                                        onPress={handleStopWave}
                                        style={[styles.rowButton, styles.stopButton]}
                                    >
                                        <Text style={styles.rowButtonText}>Stop Wave</Text>
                                    </TouchableOpacity>
                                </View>
                            </View>

                            <View style={styles.calcWrapper}>
                                <Text style={styles.calcLabel}>Calculate lap time:</Text>
                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Race distance (m)</Text>
                                    <TextInput
                                        style={styles.inputField}
                                        keyboardType="numeric"
                                        value={raceDistance}
                                        onChangeText={setRaceDistance}
                                    />
                                </View>
                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Race result (min:sec)</Text>
                                    <TextInput
                                        style={styles.inputField}
                                        value={raceResult}
                                        onChangeText={setRaceResult}
                                    />
                                </View>
                                <View style={styles.inputRow}>
                                    <Text style={styles.inputLabel}>Lap distance (m)</Text>
                                    <TextInput
                                        style={styles.inputField}
                                        keyboardType="numeric"
                                        value={lapDistance}
                                        onChangeText={setLapDistance}
                                    />
                                </View>
                                <TouchableOpacity onPress={handleCalculate} style={styles.calculateButton}>
                                    <Text style={styles.startButtonText}>Calculate</Text>
                                </TouchableOpacity>
                                {calculatedLapTime !== "" && (
                                    <Text style={styles.resultText}>Lap time: {calculatedLapTime}</Text>
                                )}
                            </View>

                            <TouchableOpacity
                                onPress={() => {
                                    scanForPeripherals();
                                    setIsModalVisible(true);
                                }}
                                style={styles.ctaButton}
                            >
                                <Text style={styles.ctaButtonText}>Connect</Text>
                            </TouchableOpacity>
                        </View>
                    </ScrollView>
                </KeyboardAvoidingView>
            )}

            {/* Device list modal (shows "Tap on a device to connect") */}
            <DeviceModal
                closeModal={() => setIsModalVisible(false)}
                visible={isModalVisible}
                connectToPeripheral={connectToDevice}
                devices={allDevices}
            />
        </SafeAreaView>
    );
};

const styles = StyleSheet.create({
    container: { flex: 1 },

    scrollContent: {
        paddingBottom: 24,
        flexGrow: 1,
    },
    topSpacer: {
        flexGrow: 1,
    },
    mainContent: {
        justifyContent: "flex-end",
    },

    ctaButton: {
        backgroundColor: "#FF6060",
        justifyContent: "center",
        alignItems: "center",
        height: 50,
        marginHorizontal: 20,
        marginVertical: 5,
        borderRadius: 8,
    },
    ctaButtonText: {
        fontSize: 18,
        fontWeight: "bold",
        color: "white",
    },

    waveWrapper: {
        paddingHorizontal: 20,
        paddingVertical: 10,
        borderTopWidth: 1,
        borderColor: "#ddd",
    },
    sectionLabel: {
        fontSize: 18,
        fontWeight: "600",
        marginBottom: 6,
        color: "#333",
    },
    inputRow: {
        flexDirection: "row",
        alignItems: "center",
        marginBottom: 4,
    },
    inputLabel: {
        width: 140,
        fontSize: 16,
        marginRight: 8,
        color: "#333",
    },
    inputField: {
        flex: 1,
        borderWidth: 1,
        borderColor: "#ccc",
        borderRadius: 8,
        padding: 10,
    },
    inputError: {
        borderColor: "red",
    },
    errorText: {
        color: "red",
        fontSize: 12,
        marginBottom: 8,
    },
    helpText: {
        color: "#555",
        fontSize: 12,
        marginBottom: 8,
        marginLeft: 148,
    },

    buttonRow: {
        flexDirection: "row",
        gap: 10,
        marginTop: 8,
    },
    rowButton: {
        flex: 1,
        justifyContent: "center",
        alignItems: "center",
        height: 50,
        borderRadius: 8,
    },
    rowButtonText: {
        fontSize: 18,
        fontWeight: "bold",
        color: "white",
    },
    startButtonValid: {
        backgroundColor: "green",
    },
    disabledButton: {
        backgroundColor: "#ccc",
    },
    stopButton: {
        backgroundColor: "#D32F2F",
    },

    calcWrapper: {
        paddingHorizontal: 20,
        paddingTop: 10,
        borderTopWidth: 1,
        borderColor: "#ddd",
    },
    calcLabel: {
        fontSize: 16,
        marginBottom: 6,
        fontWeight: "600",
        color: "#333",
    },
    calculateButton: {
        backgroundColor: "#2196F3",
        justifyContent: "center",
        alignItems: "center",
        height: 50,
        borderRadius: 8,
        marginBottom: 10,
    },
    startButtonText: {
        fontSize: 18,
        fontWeight: "bold",
        color: "white",
    },
    resultText: {
        fontSize: 18,
        fontWeight: "600",
        textAlign: "center",
        marginVertical: 10,
        color: "#000",
    },
});

export default App;
