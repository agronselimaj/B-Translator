<?php

/**
 * Add a vote for the given translation from the given user.
 * Make sure that any previous vote is cleaned first
 * (don't allow multiple votes for the same translation).
 *
 * @param $tguid
 *   ID of the translation.
 * @param $umail
 *   The user email.
 * @param $ulng
 *   The language of the user.
 * @return
 *   The ID of the new vote, or NULL.
 */
function btr_add_vote($tguid, $umail, $ulng) {
  // Fetch the translation details from the DB.
  $db = $GLOBALS['btr_db'];
  $sql = 'SELECT * FROM {btr_translations} WHERE tguid = :tguid';
  $args = array(':tguid' => $tguid);
  $trans = $db->query($sql, $args)->fetchObject();

  // If there is no such translation, return NULL.
  if (empty($trans))  return NULL;

  // Clean any previous vote.
  $nr = _btr_clean_previous_votes($tguid, $umail, $trans->sguid, $trans->lng);

  // Add the vote.
  $vid = $db->insert('btr_votes')
    ->fields(array(
        'tguid' => $tguid,
        'umail' => $umail,
        'ulng' => $ulng,
        'time' => date('Y-m-d H:i:s', REQUEST_TIME),
      ))
    ->execute();

  // Update vote count of the translation.
  $sql = 'SELECT COUNT(*) FROM {btr_votes} WHERE tguid = :tguid';
  $count = $db->query($sql, $args)->fetchField();
  $db->update('btr_translations')
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
 * @param $umail
 *   The mail of the user.
 * @param $sguid
 *   ID of the source string.
 * @param $lng
 *   Language code of the translation.
 * @return
 *   Number of previous votes that were deleted.
 *   (Normally should be 0, but can also be 1. If it is >1,
 *   something must be wrong.)
 */
function _btr_clean_previous_votes($tguid, $umail, $sguid, $lng) {

  $db = $GLOBALS['btr_db'];

  // Get the voting mode.
  $voting_mode = variable_get('btr_voting_mode', 'single');

  $arr_tguid = array();
  if ($voting_mode == 'multiple') {
    $arr_tguid = array($tguid);
  }
  else { // ($voting_mode == 'single')
    // Get the other sibling translations (translations of the same
    // string and the same language) which the user has voted.
    $sql = '
      SELECT DISTINCT t.tguid FROM {btr_translations} t
      LEFT JOIN {btr_votes} v ON (v.tguid = t.tguid)
      WHERE t.sguid = :sguid AND t.lng = :lng AND v.umail = :umail AND v.ulng = :ulng
    ';
    $params = array(':sguid' => $sguid, ':lng' => $lng, ':umail' => $umail, ':ulng' => $lng);
    $arr_tguid = $db->query($sql, $params)->fetchCol();
  }

  if (empty($arr_tguid))  return 0;

  // Insert to the trash table the votes that will be removed.
  $query = $db->select('btr_votes', 'v')
    ->fields('v', array('vid', 'tguid', 'umail', 'ulng', 'time', 'active'))
    ->condition('umail', $umail)
    ->condition('ulng', $lng)
    ->condition('tguid', $arr_tguid, 'IN');
  $query->addExpression('NOW()', 'd_time');
  $db->insert('btr_votes_trash')->from($query)->execute();

  // Remove any votes by the user for each translation in $arr_tguid.
  $num_deleted = $db->delete('btr_votes')
    ->condition('umail', $umail)
    ->condition('ulng', $lng)
    ->condition('tguid', $arr_tguid, 'IN')
    ->execute();

  // Decrement the vote count for each translation in $arr_tguid.
  $num_updated = $db->update('btr_translations')
    ->expression('count', 'count - 1')
    ->condition('tguid', $arr_tguid, 'IN')
    ->execute();

  return $num_deleted;
}


/**
 * Add a new translation to a source string.
 *
 * @param $sguid
 *   The string ID for which a new translation should be added.
 * @param $lng
 *   The language (code) of the new translation.
 * @param $translation
 *   The new translation as a string. If the string has plural
 *   version(s) as well, they are concatenated with NULL bytes ("\0")
 *   between them.
 * @param $umail
 *   The email of the author of the translation.
 * @param $single
 *   If false, then the user can add more then one translation
 *   for the same string.
 * @return
 *   Returns the ID of the new translation ($tguid),
 *   or FALSE if no translation was added.
 */
function btr_add_translation($sguid, $lng, $translation, $umail, $single =TRUE) {
  // Don't add empty translations.
  if (trim($translation) == '')  return NULL;

  $db = $GLOBALS['btr_db'];

  // Make spacing and newlines the same in translation as in the source.
  $string = btr_get_string($sguid);
  $matches = array();
  preg_match("/^(\s*).*\S(\s*)\$/s", $string, $matches);
  $translation = $matches[1] . trim($translation) . $matches[2];

  // Look for an existing translation, if any.
  $tguid = sha1($translation . $lng . $sguid);
  $existing = btr_get_translation($tguid);

  // If this translation already exists, there is nothing to be added.
  if (!empty($existing))  return NULL;

  // Insert the new suggestion.
  $db->insert('btr_translations')
    ->fields(array(
        'sguid' => $sguid,
        'lng' => $lng,
        'translation' => $translation,
        'tguid' => $tguid,
        'count' => 1,
        'umail' => $umail,
        'ulng' => $lng,
        'time' => date('Y-m-d H:i:s', REQUEST_TIME),
      ))
    ->execute();

  if ($single) {
    // If there is another translation for the same string, by the same user,
    // the new translation should replace the old one. This is useful when
    // the user wants to correct the translation, but it limits the user to
    // only one suggested translation per string. If $single is FALSE, then
    // this limitation does not apply.
    _btr_remove_old_translation($sguid, $lng, $umail, $tguid);
  }

  // Add also a vote for the new translation.
  $vid = btr_add_vote($tguid, $umail, $lng);

  // Notify previous voters of this string that a new translation has been
  // suggested. Maybe they would like to review it and change their vote.
  _btr_notify_voters_on_new_translation($sguid, $lng, $tguid, $string, $translation);

  return $tguid;
}


/**
 * If there is another translation for the same string, by the same user,
 * the new translation should replace the old one. This is useful when
 * the user wants to correct the translation, but it limits the user to
 * only one suggested translation per string.
 *
 * @param $sguid
 *   Id of the string being translated.
 * @param $lng
 *   Language of translation.
 * @param $umail
 *   Email that identifies the user who made the translation.
 * @param $tguid
 *   Id of the new translation.
 */
function _btr_remove_old_translation($sguid, $lng, $umail, $tguid) {
  $db = $GLOBALS['btr_db'];

  // Get the old translation (if any).
  $query = 'SELECT tguid, translation
            FROM {btr_translations}
            WHERE sguid = :sguid AND lng = :lng
              AND umail = :umail AND ulng = :ulng
              AND tguid != :tguid';
  $args = array(
    ':sguid' => $sguid,
    ':lng' => $lng,
    ':umail' => $umail,
    ':ulng' => $lng,
    ':tguid' => $tguid);
  $old_trans = $db->query($query, $args)->fetchObject();
  if (!$old_trans)  return;  // if there is no old translation, we are done

  // Copy to the trash table the old translation.
  $query = $db->select('btr_translations', 't')
    ->fields('t', array('sguid', 'lng', 'translation', 'tguid', 'count', 'umail', 'ulng', 'time', 'active'))
    ->condition('tguid', $old_trans->tguid);
  $query->addExpression(':d_umail', 'd_umail', array(':d_umail' => $umail));
  $query->addExpression(':d_ulng', 'd_ulng', array(':d_ulng' => $lng));
  $query->addExpression('NOW()', 'd_time');
  $db->insert('btr_translations_trash')->from($query)->execute();

  // Remove the old translation.
  $db->delete('btr_translations')
    ->condition('tguid', $old_trans->tguid)
    ->execute();

  // Get the votes of the old translation.
  $query = "SELECT v.tguid, v.time, u.umail, u.ulng, u.uid,
                   u.name AS user_name, u.status AS user_status
            FROM {btr_votes} v
            LEFT JOIN {btr_users} u ON (u.umail = v.umail AND u.ulng = v.ulng)
            WHERE v.tguid = :tguid AND v.umail != :umail";
  $args = array(':tguid' => $old_trans->tguid, ':umail' => $umail);
  $votes = $db->query($query, $args)->fetchAll();

  // Insert to the trash table the votes that will be deleted.
  $query = $db->select('btr_votes', 'v')
    ->fields('v', array('vid', 'tguid', 'umail', 'ulng', 'time', 'active'))
    ->condition('tguid', $old_trans->tguid);
  $query->addExpression('NOW()', 'd_time');
  $db->insert('btr_votes_trash')->from($query)->execute();

  // Delete the votes belonging to the old translation.
  $db->delete('btr_votes')->condition('tguid', $old_trans->tguid)->execute();

  // Associate these votes to the new translation.
  $notification_list = array();
  foreach ($votes as $vote) {
    // Associate the vote to the new translation.
    $db->insert('btr_votes')
      ->fields(array(
          'tguid' => $tguid,
          'umail' => $vote->umail,
          'ulng' => $vote->ulng,
          'time' => $vote->time,
        ))
      ->execute();

    if ($vote->user_status != 1)  continue;   // skip non-active voters

    // Add voter to the notification list.
    $notification_list[$uid] = array(
      'uid' => $uid,
      'name' => $vote->user_name,
      'umail' => $vote->umail,
    );
  }

  _btr_notify_voters_on_translation_change($notification_list, $sguid, $old_trans->translation, $tguid);
}

/**
 * Notify the voters of a translation that the author has changed
 * the translation and their votes count now for the new translation.
 */
function _btr_notify_voters_on_translation_change($voters, $sguid, $old_translation, $tguid) {

  if (empty($voters))  return;

  $string = btr_get_string($sguid);
  $new_translation = btr_get_translation($tguid);

  $notifications = array();
  foreach ($voters as $uid => $voter) {
    $notification = array(
      'type' => 'notify-voter-on-translation-change',
      'uid' => $voter['uid'],
      'username' => $voter['name'],
      'recipient' => $voter['name'] . ' <' . $voter['umail'] . '>',
      'sguid' => $sguid,
      'string' => $string,
      'old_translation' => $old_translation,
      'new_translation' => $new_translation,
    );
    $notifications[] = $notification;
  }

  btr_queue_notifications($notifications);
}

/**
 * Notify the previous voters of a string that a new translation has been
 * submitted. Maybe they would like to review it and change their vote.
 */
function _btr_notify_voters_on_new_translation($sguid, $lng, $tguid, $string, $translation) {

  $db = $GLOBALS['btr_db'];
  $query = "SELECT u.umail, u.ulng, u.uid, u.name, u.status, t.translation
            FROM {btr_translations} t
            LEFT JOIN {btr_votes} v ON (v.tguid = t.tguid)
            LEFT JOIN {btr_users} u ON (u.umail = v.umail AND u.ulng = v.ulng)
            WHERE t.sguid = :sguid AND t.lng = :lng AND t.tguid != :tguid";
  $args = array(':sguid' => $sguid, ':lng' => $lng, ':tguid' => $tguid);
  $voters = $db->query($query, $args)->fetchAll();

  if (empty($voters))  return;

  $notifications = array();
  foreach ($voters as $voter) {
    $notification = array(
      'type' => 'notify-voter-on-new-translation',
      'uid' => $voter->uid,
      'username' => $voter->name,
      'recipient' => $voter->name . ' <' . $voter->umail . '>',
      'sguid' => $sguid,
      'string' => $string,
      'voted_translation' => $voter->translation,
      'new_translation' => $translation,
    );
    $notifications[] = $notification;
  }

  btr_queue_notifications($notifications);
}


/**
 * Delete the translation with the given id and any related votes.
 *
 * Parameters $umail and $ulng identify the user that is making
 * the deletion.
 */
function btr_del_translation($tguid, $umail, $ulng) {
  $db = $GLOBALS['btr_db'];

  // Before deleting, get the author and voters (for notifications).
  list($author, $voters) = _btr_get_author_and_voters($tguid);

  // Copy to the trash table the translation that will be deleted.
  $query = $db->select('btr_translations', 't')
    ->fields('t', array('sguid', 'lng', 'translation', 'tguid', 'count', 'umail', 'ulng', 'time', 'active'))
    ->condition('tguid', $tguid);
  $query->addExpression(':d_umail', 'd_umail', array(':d_umail' => $umail));
  $query->addExpression(':d_ulng', 'd_ulng', array(':d_ulng' => $ulng));
  $query->addExpression('NOW()', 'd_time');
  $db->insert('btr_translations_trash')->from($query)->execute();

  // Copy to the trash table the votes that will be deleted.
  $query = $db->select('btr_votes', 'v')
    ->fields('v', array('vid', 'tguid', 'umail', 'ulng', 'time', 'active'))
    ->condition('tguid', $tguid);
  $query->addExpression('NOW()', 'd_time');
  $db->insert('btr_votes_trash')->from($query)->execute();

  // Delete the translation and any votes related to it.
  $db->delete('btr_translations')->condition('tguid', $tguid)->execute();
  $db->delete('btr_votes')->condition('tguid', $tguid)->execute();

  // Notify the author of a translation and its voters
  // that it has been deleted.
  _btr_notify_voters_on_translation_del($tguid, $author, $voters);
}

/**
 * Before deleting a translation, get the author and voters (for notifications).
 */
function _btr_get_author_and_voters($tguid) {
  $get_author =
    "SELECT u.uid, u.name, u.umail
     FROM {btr_translations} t
     LEFT JOIN {btr_users} u
         ON (u.umail = t.umail AND u.ulng = t.ulng)
     WHERE t.tguid = :tguid";

  $get_voters =
    "SELECT u.uid, u.name, u.umail
     FROM {btr_votes} v
     LEFT JOIN {btr_users} u
         ON (u.umail = v.umail AND u.ulng = v.ulng)
     WHERE v.tguid = :tguid";

  $args = array(':tguid' => $tguid);

  $db = $GLOBALS['btr_db'];
  $author = $db->query($get_author, $args)->fetchObject();
  $voters = $db->query($get_voters, $args)->fetchAll();

  return array($author, $voters);
}


/**
 * Notify the author of a translation and its voters
 * that it has been deleted.
 */
function _btr_notify_voters_on_translation_del($tguid, $author, $voters) {
  // get the sguid, string and translation
  $sql = 'SELECT sguid FROM {btr_translations} WHERE tguid = :tguid';
  $args = array(':tguid' => $tguid);
  $sguid = $GLOBALS['btr_db']->query($sql, $args)->fetchField();
  $string = btr_get_string($sguid);
  $translation = btr_get_translation($tguid);

  $notifications = array();

  // Notify the author of the translation about the deletion.
  $notification = array(
    'type' => 'notify-author-on-translation-deletion',
    'uid' => $author->uid,
    'username' => $author->name,
    'recipient' => $author->name . ' <' . $author->umail . '>',
    'sguid' => $sguid,
    'string' => $string,
    'translation' => $translation,
  );
  $notifications[] = $notification;

  // Notify the voters of the translation as well.
  foreach ($voters as $voter) {
    if ($voter->name == $author->name)  continue;  // don't send a second message to the author
    $notification = array(
      'type' => 'notify-voter-on-translation-deletion',
      'uid' => $voter->uid,
      'username' => $voter->name,
      'recipient' => $voter->name . ' <' . $voter->umail . '>',
      'sguid' => $sguid,
      'string' => $string,
      'translation' => $translation,
    );
    $notifications[] = $notification;
  }

  btr_queue_notifications($notifications);
}
