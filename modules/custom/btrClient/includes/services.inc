<?php
/**
 * Define web services.
 */

/**
 * Implements hook_default_wsclient_service().
 *
 * Defines web service descriptions (one public and one private).
 * This hook is invoked when web service descriptions
 * are loaded like this:
 *     $btr = wsclient_service_load('dev.l10n.org.xx');
 *     $result = $btr->statistics('sq');
 *
 * @return
 *     An array of web service descriptions
 *     with the web service names as keys.
 */
function btrClient_default_wsclient_service() {

  $services['public_btr'] = _btrClient_services_public_service('public_btr');
  $services['btr'] = _btrClient_services_authenticated_service('btr');
  return $services;
}

/**
 * Create and return the public service.
 */
function _btrClient_services_public_service($service_name) {
  $service = new WSClientServiceDescription();
  $service->name = $service_name;
  $service->label = 'Public B-Translator Services';
  $service->type = 'rest';
  $service->url = $server_url . '/public/btr/';
  _btrClient_services_development_settings(&$service);

  $service->operations['report_statistics'] = array(
    'label' => 'report_statistics',
    'url' => 'report/statistics',
    'parameter' => array(
      'lng' => array('type' => 'text'),
    ),
  );
  $service->operations['report_topcontrib'] = array(
    'label' => 'report_topcontrib',
    'url' => 'report/topcontrib',
    'parameter' => array(
      'lng' => array('type' => 'text'),
      'period' => array('type' => 'text'),
      'size' => array('type' => 'integer'),
    ),
  );
  $service->operations['report_statistics_1'] = array(
    'label' => 'report_statistics_1',
    'url' => 'report/statistics',
    'type' => 'POST',
    'data' => 'params',
    'parameter' => array(
      'params' => array(),
    ),
  );
  $service->operations['report_topcontrib_1'] = array(
    'label' => 'report_topcontrib_1',
    'url' => 'report/topcontrib',
    'type' => 'POST',
    'data' => 'params',
    'parameter' => array(
      'params' => array(),
    ),
  );
  $service->operations['get_translations'] = array(
    'label' => 'get_translations',
    'url' => 'translations/@sguid',
    'parameter' => array(
      'sguid' => array('type' => 'text'),
      'lng' => array('type' => 'text'),
    ),
  );

  return $service;
}

/**
 * Create and return the authenticated service.
 */
function _btrClient_services_authenticated_service($service_name) {
  $server_url = variable_get('oauth2_login_oauth2_server', '');
  $client_id = variable_get('oauth2_login_client_id', '');
  $client_secret = variable_get('oauth2_login_client_secret', '');

  $token_endpoint = $server_url . '/oauth2/token';
  $authorization_endpoint = $server_url . '/oauth2/authorize';
  $redirect_uri = url('oauth2/authorized', array('absolute' => TRUE));

  $service = new WSClientServiceDescription();
  $service->name = $service_name;
  $service->label = 'Authenticated B-Translator Services';
  $service->type = 'rest';
  $service->url = $server_url . '/btr/';

  _btrClient_services_development_settings(&$service);

  $service->settings['authentication']['oauth2'] = array(
    'token_endpoint' => $token_endpoint,
    // auth_flow can be: server-side | client-credentials | user-password
      'auth_flow' => 'server-side',
    'client_id' => $client_id,
    'client_secret' => $client_secret,
    //'username' => 'user1',
    //'password' => 'user1',
    'redirect_uri' => $redirect_uri,
    'authorization_endpoint' => $authorization_endpoint,
    //'scope' => '',
    //'query_auth' => TRUE,
  );

  $service->operations['get_translations'] = array(
    'label' => 'get_translations',
    'url' => 'translations/@sguid',
    'parameter' => array(
      'sguid' => array('type' => 'text'),
      'lng' => array('type' => 'text'),
    ),
  );

  return $service;
}

/**
 * Set curl options for development, testing and debug.
 */
function _btrClient_services_development_settings(&$service) {
  $skipssl = variable_get('oauth2_login_skipssl', TRUE);
  $proxy = variable_get('oauth2_login_proxy', '');

  if ($skipssl) {
    // skip checking the SSL certificate, for testing
    $service->settings['curl options'][CURLOPT_SSL_VERIFYPEER] = FALSE;
    $service->settings['curl options'][CURLOPT_SSL_VERIFYHOST] = FALSE;
  }
  if ($proxy) {
    $service->settings['curl options'][CURLOPT_PROXY] = $proxy;
  }
}