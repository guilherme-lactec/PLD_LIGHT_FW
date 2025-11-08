#include "DashboardServer.h"
#include "time.h" // Para buscar a hora NTP

// ATUALIZADO: HTML com slider de aceleração dentro do formulário
const char *DashboardServer::_dashboard_html = R"EOF(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 0; }
        .container { max-width: 600px; margin: 30px auto; padding: 20px; background-color: #fff; border-radius: 10px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        h1 { text-align: center; color: #333; }
        .card-grid { display: grid; grid-template-columns: 1fr; gap: 20px; margin-top: 20px; }
        .card { background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 8px; padding: 15px; }
        .card h2 { margin-top: 0; color: #0056b3; }
        .data { font-size: 2.5em; font-weight: bold; color: #333; text-align: center; margin: 10px 0; }
        
        /* Cores */
        #data-hora h2 { color: #5cb85c; }
        #settings-card h2 { color: #777; }
        #temperatura { color: #d9534f; }
        #humidade { color: #337ab7; }
        #luminosidade { color: #f0ad4e; }

        /* Estilo para o formulário */
        .form-group { display: flex; justify-content: space-between; align-items: center; margin: 20px 0; }
        .form-group label { font-weight: bold; font-size: 1.1em; }
        .form-group input[type="time"] { border: 1px solid #ccc; border-radius: 4px; padding: 8px; font-size: 1.1em; }
        .form-group input[type="range"] { flex-grow: 1; margin: 0 15px; }
        .form-group button { background-color: #007bff; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; font-size: 1em; }
        .form-group button:hover { background-color: #0056b3; }
        #acelValor { font-size: 1.1em; font-weight: bold; min-width: 40px; text-align: right; }

    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Dashboard</h1>
        
        <div class="card-grid">
            <div id="data-hora" class="card">
                <h2>Data & Hora (NTP)</h2>
                <div id="date" class="data">--/--/----</div>
                <div id="time" class="data">--:--:--</div>
            </div>

            <div id="settings-card" class="card">
                <h2>Configura&ccedil;&otilde;es</h2>
                <form id="formSettings">
                    <div class="form-group">
                        <label for="horaLigar">Ligar Luz &agrave;s:</label>
                        <input type="time" id="horaLigar" name="ligar" required>
                    </div>
                    <div class="form-group">
                        <label for="horaDesligar">Desligar Luz &agrave;s:</label>
                        <input type="time" id="horaDesligar" name="desligar" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="aceleracao">Acelera&ccedil;&atilde;o:</label>
                        <input type="range" id="aceleracao" name="aceleracao" min="0" max="100" value="50">
                        <span id="acelValor">50</span>
                    </div>

                    <div class="form-group">
                        <span></span> <button type="submit">Salvar Configura&ccedil;&otilde;es</button>
                    </div>
                </form>
            </div>

            <div class="card"><h2 style="color:#d9534f">Temperatura</h2><div id="temperatura" class="data">--.-- &deg;C</div></div>
            <div class="card"><h2 style="color:#337ab7">Humidade</h2><div id="humidade" class="data">--.-- %</div></div>
            <div class="card"><h2 style="color:#f0ad4e">Luminosidade</h2><div id="luminosidade" class="data">---- lux</div></div>

        </div>
    </div>
    
    <script>
        // --- Atualiza o valor de aceleração no <span> ao arrastar o slider ---
        var slider = document.getElementById("aceleracao");
        var output = document.getElementById("acelValor");
        output.innerHTML = slider.value; // Mostra o valor padrão
        slider.oninput = function() {
            output.innerHTML = this.value;
        }

        // --- Função para BUSCAR dados do ESP32 (GET) ---
        function fetchData() {
            fetch('/data.json')
                .then(response => response.json())
                .then(data => {
                    // Atualiza Data e Hora
                    document.getElementById('date').innerText = data.date;
                    document.getElementById('time').innerText = data.time;
                    
                    // Atualiza Sensores
                    document.getElementById('temperatura').innerHTML = parseFloat(data.temperatura).toFixed(1) + ' &deg;C';
                    document.getElementById('humidade').innerHTML = parseFloat(data.humidade).toFixed(1) + ' %';
                    document.getElementById('luminosidade').innerHTML = parseInt(data.luminosidade) + ' lux';

                    // Atualiza os valores das Configurações salvas
                    document.getElementById('horaLigar').value = data.hora_ligar;
                    document.getElementById('horaDesligar').value = data.hora_desligar;
                    document.getElementById('aceleracao').value = data.aceleracao;
                    document.getElementById('acelValor').innerHTML = data.aceleracao;
                })
                .catch(error => { console.error('Erro ao buscar dados:', error); });
        }
        
        // --- Função para ENVIAR dados para o ESP32 (POST) ---
        document.getElementById('formSettings').addEventListener('submit', function(e) {
            e.preventDefault(); // Impede o recarregamento da página

            // Envia os dados do formulário
            fetch('/settings', {
                method: 'POST',
                body: new FormData(this) // Envia (ligar, desligar E aceleracao)
            })
            .then(response => {
                if(response.ok) {
                    alert('Configurações salvas!');
                } else {
                    alert('Erro ao salvar.');
                }
            });
        });

        // Loop de atualização
        fetchData();
        setInterval(fetchData, 2000); // Intervalo de atualização
    </script>
</body>
</html>
)EOF";

DashboardServer::DashboardServer(int port) : _server(port)
{
    _dataCallback = nullptr;
    _settingsCallback = nullptr;
}

void DashboardServer::begin()
{
    _server.on("/", HTTP_GET, std::bind(&DashboardServer::handleRoot, this));
    _server.on("/data.json", HTTP_GET, std::bind(&DashboardServer::handleDataJson, this));

    // Rota POST para receber as configurações
    _server.on("/settings", HTTP_POST, std::bind(&DashboardServer::handleSettings, this));

    _server.begin();
    Serial.println("Servidor de Dashboard iniciado!");
}

void DashboardServer::loop()
{
    _server.handleClient();
}

void DashboardServer::onDataRequest(DataCallback callback)
{
    _dataCallback = callback;
}

// Armazena o callback de settings
void DashboardServer::onSettingsRequest(SettingsCallback callback)
{
    _settingsCallback = callback;
}

// --- Handlers Privados ---

void DashboardServer::handleRoot()
{
    _server.send(200, "text/html", _dashboard_html);
}

void DashboardServer::handleDataJson()
{
    StaticJsonDocument<512> doc;

    // 1. A biblioteca cuida da hora
    struct tm timeinfo;
    char dateStr[20];
    char timeStr[20];
    if (getLocalTime(&timeinfo))
    {
        strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    }
    else
    {
        strcpy(dateStr, "Sincronizando...");
        strcpy(timeStr, "--:--:--");
    }
    doc["date"] = dateStr;
    doc["time"] = timeStr;

    // 2. Chama o callback do main.cpp para adicionar
    //    todos os outros dados (sensores E configurações salvas)
    if (_dataCallback != nullptr)
    {
        _dataCallback(doc);
    }
    else
    {
        // Fallback
        doc["temperatura"] = 0;
        doc["humidade"] = 0;
        doc["luminosidade"] = 0;
        doc["aceleracao"] = 50; // Valor de fallback
        doc["hora_ligar"] = "00:00";
        doc["hora_desligar"] = "00:00";
    }

    String output;
    serializeJson(doc, output);
    _server.send(200, "application/json", output);
}

// ATUALIZADO: Handler para o POST /settings
void DashboardServer::handleSettings()
{
    // Verifica se o callback foi registrado e se os 3 argumentos chegaram
    if (_settingsCallback &&
        _server.hasArg("ligar") &&
        _server.hasArg("desligar") &&
        _server.hasArg("aceleracao"))
    {

        // Pega os valores
        String ligar = _server.arg("ligar");
        String desligar = _server.arg("desligar");
        int aceleracao = _server.arg("aceleracao").toInt(); // Converte para int

        // Chama o callback no main.cpp para que ele salve os valores
        _settingsCallback(ligar, desligar, aceleracao);

        _server.send(200, "text/plain", "OK"); // Responde ao navegador que deu certo
    }
    else
    {
        _server.send(400, "text/plain", "Bad Request");
    }
}