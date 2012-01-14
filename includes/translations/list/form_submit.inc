<?php

// = Translation form submission ===============================================

/**
 * Form submit callback for l10n_suggestions_translate_form().
 *
 * @see l10n_suggestions_translate_form().
 */
function l10n_suggestions_translate_form_submit($form, &$form_state) {
  $form_values = $form_state['values'];
  $lng = $form_values['langcode'];
  // Get the voting mode.
  $voting_mode = variable_get('l10n_suggestions_voting_mode', 'single');

  // Iterate outer structure built in l10n_suggestions_translate_form().
  foreach ($form_values['strings'] as $sguid => $string) {

    if ($voting_mode == 'single') {
      $tguid = $string['approved'];
      _l10n_suggestions_translate_form_submit_single($sguid, $lng, $string);
    }

    // Iterate inner structure built in _l10n_suggestions_translate_form_item().
    // Form items have numeric $tguid values and other keys here.
    foreach ($string as $tguid => $translation) {
      if ($voting_mode == 'multiple') {
	_l10n_suggestions_translate_form_submit_multiple($sguid, $tguid, $lng, $translation);
      }
      if ((strlen($tguid) == 40) && !empty($translation['declined'])) {
	//do something for declined translations
      }
    }
  }

  /*
  // TODO:
  // Tell the user what happened.
  l10n_suggestions_update_message();
  */

  // Keep existing filters and other query arguments on form submission.
  $redirect_args = $form_state['values']['target'];
  unset($redirect_args['q']);
  $form_state['redirect'] = array($form_state['values']['target']['q'], array('query' => $redirect_args));
}

function _l10n_suggestions_translate_form_submit_single($sguid, $lng, $string) {

  $tguid = $string['approved'];
  if ($tguid == '')  return;

  if ($tguid == 'new') {
    //add a new suggestion
    $values = $string[$tguid]['value'];
    l10n_suggestions_add_suggestion($sguid, $lng, $values);
    return;
  }

  if (strlen($tguid) == 40) {
    // If this is not an existing vote,
    // then add a new vote for this translation.
    $previous_votes = $string[$tguid]['original']->votes;
    global $user;
    if (!in_array($user->uid, array_keys($previous_votes))) {
      l10n_suggestions_add_vote($tguid, $user->uid);
    }
  }
}

function _l10n_suggestions_translate_form_submit_multiple($sguid, $tguid, $lng, $translation) {
  global $user;

  $approved = $translation['approved'];
  if ($tguid == 'new') {
    //add a new suggestion
    $values = isset($translation['value']) ? $translation['value'] : array();
    l10n_suggestions_add_suggestion($sguid, $lng, $values);
  }
  elseif ($approved != '') {
    // Add a new vote for this translation
    // if such a vote does not exist.
    $previous_votes = $translation['original']->votes;
    if (!in_array($user->uid, array_keys($previous_votes))) {
      l10n_suggestions_add_vote($tguid, $user->uid);
    }
  }
  elseif ($approved == '') {
    // Remove this vote, if it exists.
    $previous_votes = $translation['original']->votes;
    if (in_array($user->uid, array_keys($previous_votes))) {
      _l10n_suggestions_clean_previous_votes($tguid, $user->uid, $sguid, $lng);
    }
  }
}

/**
 * Add a vote for the given translation from the given user.
 * Make sure that any previous vote is cleaned first
 * (don't allow multiple votes for the same translation).
 *
 * @param $tguid
 *   ID of the translation.
 * @param $uid
 *   ID of the user.
 * @return
 *   The ID of the new vote, or NULL.
 */
function l10n_suggestions_add_vote($tguid, $uid) {
  // Fetch the translation details from the DB.
  $sql = 'SELECT * FROM {l10n_suggestions_translations} WHERE tguid = :tguid';
  $trans = db_query($sql, array(':tguid' => $tguid))->fetchObject();

  // If there is no such translation, return NULL.
  if (empty($trans))  return NULL;

  // Clean any previous vote.
  $nr = _l10n_suggestions_clean_previous_votes($tguid, $uid, $trans->sguid, $trans->lng);

  // Add the vote.
  $vid = db_insert('l10n_suggestions_votes')
    ->fields(array(
        'tguid' => $tguid,
        'uid' => $uid,
        'time' => date('Y-m-d H:i:s', REQUEST_TIME),
      ))
    ->execute();

  // Update vote count of the translation.
  $sql = 'SELECT COUNT(*) FROM {l10n_suggestions_votes} WHERE tguid = :tguid';
  $count = db_query($sql, array(':tguid' => $tguid))->fetchField();
  db_update('l10n_suggestions_translations')
    ->fields(array('count' => $count))
    ->condition('tguid', $tguid)
    ->execute();

  return $vid;
}

/**
 * Clean any previous vote by this user for this translation.
 *
 * This depends on the voting mode option (set by the admin).
 * If the voting mode is 'single', then the user can select
 * only one translation for a given string (at most one vote
 * per string).
 * If the voting mode is 'multiple', then the user can approve
 * several translations for a string (at most one vote per
 * translation).
 *
 * @param $tguid
 *   ID of the translation.
 * @param $uid
 *   ID of the user.
 * @param $sguid
 *   ID of the source string.
 * @param $lng
 *   Language code of the translation.
 * @return
 *   Number of previous votes that were deleted.
 *   (Normally should be 0, but can also be 1. If it is >1,
 *   something must be wrong.)
 */
function _l10n_suggestions_clean_previous_votes($tguid, $uid, $sguid, $lng) {

  //TODO: Instead of deleting previous votes, save them in
  //      an archive table, so that the history of changing
  //      votes can be preserved.

  //TODO: Add the configuration variable l10n_suggestions_voting_mode
  //      which can be changed by the admin.

  // Get the voting mode.
  $voting_mode = variable_get('l10n_suggestions_voting_mode', 'single');

  if ($voting_mode == 'multiple') {
    // Remove any other votes by the user for this translation.
    $num_deleted = db_delete('l10n_suggestions_votes')
      ->condition('uid', $uid)
      ->condition('tguid', $tguid)
      ->execute();

    return $num_deleted;
  }
  else { // ($voting_mode == 'single')
    // Get the other sibling translations
    // (translations of the same string for the same language).
    $sql = 'SELECT tguid FROM {l10n_suggestions_translations} '
      . 'WHERE sguid = :sguid AND lng = :lng';
    $params = array(':sguid' => $sguid, ':lng' => $lng);
    $arr_tguid = db_query($sql, $params)->fetchCol();

    if (empty($arr_tguid))  return 0;

    // Remove any other votes by the user for this string.
    $num_deleted = db_delete('l10n_suggestions_votes')
      ->condition('uid', $uid)
      ->condition('tguid', $arr_tguid, 'IN')
      ->execute();

    return $num_deleted;
  }
}

/**
 * Adds a new translation suggestion to a source string.
 *
 * @param $sguid
 *   The string ID for which a new suggestion should be added.
 * @param $lng
 *   The language code of the new suggestion.
 * @param $values
 *   Array of strings containing the new translation suggestion
 *   for the singular version and for plural version(s) (if any).
 * @param $uid
 *   ID of the user that submitted the suggestion. If missing,
 *   the logged in user will be used.
 */
function l10n_suggestions_add_suggestion($sguid, $lng, $values, $uid =NULL) {

  if ($uid == NULL) {
    global $user;
    $uid = $user->uid;
  }

  $translation = l10n_suggestions_pack_string($values);
  $translation = str_replace(t('<New translation>'), '', $translation);

  // Don't add empty translations.
  if (trim($translation) == '')  return false;

  // Load the string and adjust translation whitespace based on source.
  $query = 'SELECT string FROM {l10n_suggestions_strings} WHERE sguid = :sguid';
  $source_string = db_query($query, array(':sguid' => $sguid))->fetchField();
  $translation = l10n_suggestions_trim($translation, $source_string);

  // Look for an existing translation, if any.
  $tguid = sha1($translation . $lng . $sguid);
  $query = 'SELECT translation FROM {l10n_suggestions_translations} WHERE tguid = :tguid';
  $existing = db_query($query, array(':tguid' => $tguid))->fetchField();

  // If this translation already exists, there is nothing to be added.
  if (!empty($existing))  return false;

  // Insert the new suggestion.
  db_insert('l10n_suggestions_translations')
    ->fields(array(
        'sguid' => $sguid,
        'lng' => $lng,
        'translation' => $translation,
	'tguid' => $tguid,
        'count' => 1,
        'uid' => $uid,
        'time' => date('Y-m-d H:i:s', REQUEST_TIME),
      ))
    ->execute();

  // Add also a vote for the new suggestion.
  $vid = l10n_suggestions_add_vote($tguid, $uid);

  //TODO: Notify previous voters of this string that a new suggestion
  //      has been submitted. Maybe they would like to review it and
  //      change their vote.

  return $tguid;
}

/**
 * Make spacing and newlines the same in translation as in the source.
 *
 * @param $translation
 *   Translation string.
 * @param $source
 *   Source string.
 * @return
 *   Translation string with the right beginning and ending chars.
 */
function l10n_suggestions_trim($translation, $source) {
  $matches = array();
  preg_match("/^(\s*).*\S(\s*)\$/s", $source, $matches);
  return $matches[1] . trim($translation) . $matches[2];
}

?>