<?php
/**
 * @file
 * btr_captcha.features.user_permission.inc
 */

/**
 * Implements hook_user_default_permissions().
 */
function btr_captcha_user_default_permissions() {
  $permissions = array();

  // Exported permission: administer CAPTCHA settings.
  $permissions['administer CAPTCHA settings'] = array(
    'name' => 'administer CAPTCHA settings',
    'roles' => array(
      0 => 'administrator',
    ),
    'module' => 'captcha',
  );

  // Exported permission: administer honeypot.
  $permissions['administer honeypot'] = array(
    'name' => 'administer honeypot',
    'roles' => array(
      0 => 'administrator',
    ),
    'module' => 'honeypot',
  );

  // Exported permission: administer recaptcha.
  $permissions['administer recaptcha'] = array(
    'name' => 'administer recaptcha',
    'roles' => array(
      0 => 'administrator',
    ),
    'module' => 'recaptcha',
  );

  // Exported permission: bypass honeypot protection.
  $permissions['bypass honeypot protection'] = array(
    'name' => 'bypass honeypot protection',
    'roles' => array(
      0 => 'administrator',
    ),
    'module' => 'honeypot',
  );

  // Exported permission: skip CAPTCHA.
  $permissions['skip CAPTCHA'] = array(
    'name' => 'skip CAPTCHA',
    'roles' => array(
      0 => 'administrator',
    ),
    'module' => 'captcha',
  );

  return $permissions;
}
