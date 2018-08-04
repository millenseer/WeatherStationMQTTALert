<?php
  $text = "";
  if($_GET["text"] != "") {
    $text = $_GET["text"];
  }
?>
<html>  
  <head>      
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <title>LED for ESP8266</title>

    <script src="https://code.jquery.com/jquery-2.1.4.min.js"></script>
    <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/js/bootstrap.min.js"></script>
    <link href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap.min.css" rel="stylesheet">
    <link rel="stylesheet" href="//maxcdn.bootstrapcdn.com/font-awesome/4.3.0/css/font-awesome.min.css">
    <link rel="stylesheet" href="customized-bootstrap.css">

  </head>
  <body>
    <div class="row" style="margin-top: 20px;">
      <div class="col-md-8 col-md-offset-2">
<?php

require("./phpMQTT.php");

$username = "";                   // set your username
$password = "";                   // set your password

$statusmsg = "";
$mqttserver = "localhost";
$mqttport = 1883;
$mqttpubkeepalive = 5;
$mqttsubkeepalive = 60;
$pubtopic = "LIGHTS";
$client_id = "phpMQTT-publisher"; // make sure this is unique for connecting to sever - you could use uniqid()

//var_dump($_POST);

$fertigeNachrichten[] = "LEISE SEIN";
$fertigeNachrichten[] = "Auto auspacken!";
$fertigeNachrichten[] = "Ab in die Falle!";
$fertigeNachrichten[] = "Essen kommen!";
$fertigeNachrichten[] = "Sofort runterkommen!";

$response = "";
if($_GET["light"] == "on") {
  $response = "LIGHT-1|Publishing ON to MQTT|";
  publish_message('LIGHT-1:ON', $pubtopic, $mqttserver, $mqttport, $client_id);
  $response = $response . "End";
}     
if($_GET["light"] == "off") {
  $response = "LIGHT-1|Publishing OFF to MQTT|";
  publish_message('LIGHT-1:OFF', $pubtopic, $mqttserver, $mqttport, $client_id);
  $response = $response . "End";
}    
if($_GET["light"] == "blink") {
  $response = "LIGHT-1|Publishing BLINK to MQTT|";
  publish_message('LIGHT-1:BLINK', $pubtopic, $mqttserver, $mqttport, $client_id);
  $response = $response . "End";
}    
if($_GET["text"] != "") {
  $text = $_GET["text"];
  $response = "TEXT-1|Publishing '$text' to MQTT|";
  publish_message($text, "TEXT", $mqttserver, $mqttport, $client_id);
  $response = $response . "End";
}    
                        
if (!empty($statusmsg)) {
  echo '<div style="background:#FFCC00; color: #111111;border: 2px solid grey; border-radius: 3px;padding: 5px;">';
  echo $statusmsg;
  echo '</div>';
}

?>

<h1>MaSaMaJu - Pager<span class="badge badge-secondary">extra nervig</span></h1>
<form>
  <div class="form-group">
    <label for="exampleFormControlTextarea1">Textnachricht</label>
    <textarea name="text" class="form-control" id="exampleFormControlTextarea1" rows="2"><?php echo $text; ?></textarea>
  </div>
  <button type="submit" class="btn btn-primary">Senden</button>
</form>
<form id="Buttons">
<label for="fertigeNachrichten">Fertige Nachrichten</label><br />
<p>
<?php 
foreach ($fertigeNachrichten as &$value) {
?>
  <button type="submit" class="btn btn-warning" name="text" value="<?php echo $value ?>"><?php echo $value ?></button>
<?php
}
?>
</p>
</form>
        <a href="?light=blink" class="btn btn-success btn-block btn-lg">LED nervig blinken</a>
        <br />
        <a href="?light=on" class="btn btn-success btn-block btn-lg">LED EIN</a>
        <br />
        <a href="?light=off" class="led btn btn-danger btn-block btn-lg">Alarm aus</a>
        <br />
        <div class="light-status well" style="margin-top: 5px; text-align:center">
          

<?php
if (!empty($response)) {
  echo '<div style="background:none; color: #999999;border: 0px solid grey; border-radius: 3px;padding: 5px;">';
  echo $response;
  echo '</div>';
}

function publish_message($msg, $topic, $server, $port, $client_id) {
  $mqtt = new phpMQTT($server, $port, $client_id);
  if ($mqtt->connect(true, NULL, NULL, NULL)) {
    $mqtt->publish($topic, $msg, 0);
    $mqtt->close();
  } else {
    echo "Time out!\n";
  }
}

?>
        </div>
      </div>
    </div>
  </body>
</html>  

