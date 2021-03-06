<?php
/**
 * @file
 * Import strings and translations from POT/PO files of projects.
 */

/**
 * Menu callback: translations/addterm
 * Add a new term on the vocabulary.
 */
function btrClient_addterm() {
  $output[] = drupal_get_form('btrClient_addterm_form');
  if(user_access('administer')) {
    $output[] = drupal_get_form('btrClient_delterm_form');
  }
  return $output;
}

/**
 * Form: addterm
 */
function btrClient_addterm_form() {
  $form = array(
    '#prefix' => '<div class="btr-ui-filter clear-block">',
    '#suffix' => '</div>',
  );

  $form['addterm'] = array(
    '#type' => 'fieldset',
    '#title' => t('Add a New Term'),
    '#description' => t('Add a new interesting term on the vocabulary.'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $form['addterm']['newterm'] = array(
    '#type' => 'textfield',
    '#title' => t('New Term'),
    '#required' => TRUE,
    //'#description' => t('The origin of the project.'),
  );

  $form['addterm']['buttons'] = array(
    //'#prefix' => '<div class="export-submit">',
    //'#suffix' => '</div>',
  );
  $form['addterm']['buttons']['submit'] = array(
    '#value' => t('Add'),
    '#type' => 'submit',
  );

  return $form;
}

/**
 * Submission handler for addterm form.
 */
function btrClient_addterm_form_submit($form, &$form_state) {
  //dpm($form_state['values']);  //debug
  $string = check_plain(trim($form_state['values']['newterm']));

  $sguid = btr_add_term($string);
  if (empty($sguid)) {
    drupal_set_message(t('This term already exists.'));
    return;
  }

  // notify translators about the new term
  btrClient_new_term_notification($string, $sguid);

  // redirect to translating the term
  $lng = variable_get('btrClient_translation_lng', 'fr');
  drupal_goto("translations/$lng/$sguid");
}

/**
 * Notify translators about the new term.
 */
function btrClient_new_term_notification($string, $sguid) {

  // Get all the users with role 'translator'.
  $query = "SELECT ur.uid FROM users_roles ur
            RIGHT JOIN role r ON (ur.rid = r.rid)
            WHERE r.name = 'translator' ";
  $uids = db_query($query)->fetchCol();
  $translators = user_load_multiple($uids);

  // Notify the translators about the new term.
  $queue = DrupalQueue::get('notifications');
  $queue->createQueue();  // There is no harm in trying to recreate existing.
  foreach ($translators as $translator) {
    $notification_params = array(
      'type' => 'notify-translator-on-new-term',
      'uid' => $translator->uid,
      'username' => $translator->name,
      'recipient' => $translator->name . ' <' . $translator->mail . '>',
      'sguid' => $sguid,
      'string' => $string,
      'author' => $GLOBALS['user']->name,
    );
    $queue->createItem((object)$notification_params);
  }
}


/**
 * Form: delterm
 */
function btrClient_delterm_form() {
  $form = array(
    '#prefix' => '<div class="btr-ui-filter clear-block">',
    '#suffix' => '</div>',
  );

  $form['delterm'] = array(
    '#type' => 'fieldset',
    '#title' => t('Delete a Term'),
    '#description' => t('Delete a term from the vocabulary.'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $form['delterm']['sguid'] = array(
    '#type' => 'textfield',
    '#title' => t('String ID'),
    '#required' => TRUE,
    //'#description' => t('The origin of the project.'),
  );

  $form['delterm']['buttons'] = array(
    //'#prefix' => '<div class="export-submit">',
    //'#suffix' => '</div>',
  );
  $form['delterm']['buttons']['submit'] = array(
    '#value' => t('Delete'),
    '#type' => 'submit',
  );

  return $form;
}

/**
 * Submission handler for delterm form.
 */
function btrClient_delterm_form_submit($form, &$form_state) {

  $sguid = check_plain(trim($form_state['values']['sguid']));
  btr_del_term($sguid);

  // redirect to the list of terms
  $lng = variable_get('btrClient_translation_lng', 'fr');
  drupal_goto("translations/search",
    array('query' => array(
        'lng' => $lng,
        'project' => 'vocabulary',
        'limit' => 10,
      )));
}


/**
 * Menu callback: translations/import
 * Import strings and translations from POT/PO files of projects.
 */
function btrClient_import() {
  return 'Not implemented yet.';
}
