(ql:quickload :cl-redis)

(defmacro with-connection (&body body)
  (when (not (redis:connected-p))
    (redis:connect))
  `(progn ,@body))

(defun get-all-utxos ()
  (with-connection
      (red:smembers "znn:utxos")))

(defparameter *utxos* (get-all-utxos))

(defun refresh-utxos ()
  (setq *utxos* (get-all-utxos)))

(defun get-blocks-referencing (address)
  (with-connection
      (red:smembers (concatenate 'string "znn:blocks:" address))))

(defun get-change-at-block (address block)
  (with-connection
      (red:get (format nil "znn:change:~A:~A" address block))))

(defun to-numbers (number-string-seq)
  (mapcar #'read-from-string number-string-seq))

(defun get-balance-at-block (address block)
  (let ((blocks (get-blocks-referencing address)))
    (loop for blocknum in (sort (to-numbers blocks) #'<)
          while (< blocknum block)
          collecting (get-change-at-block address blocknum) into changes
          finally (return (reduce #'+ (to-numbers changes))))))

(defun get-max-block-height ()
  (with-connection
      (read-from-string (red:get "znn:blocks:top"))))

(defun random-utxo ()
  (nth (random (length *utxos*)) *utxos*))

(defun string-to-znn.zat (numberstring)
  (let* ((zero-znn (< (length numberstring) 8))
         (znn (if zero-znn
                  "0"
                  (subseq numberstring 0 (- (length numberstring) 8))))
        (zats (if zero-znn
                  (format nil "~8,'0d" (read-from-string numberstring))
                  (subseq numberstring (- (length numberstring) 8)))))
    (read-from-string (format nil "~A.~A" znn zats))))

(defun number-to-znn.zat (number)
  (string-to-znn.zat (write-to-string number)))

;;(number-to-znn.zat 12345678)
;;(number-to-znn.zat 123456789)
;;(number-to-znn.zat 1234)

(loop for utxo in (subseq *utxos* 0 200)
      do
         (format t "~A has ~A ZNN~%" utxo (number-to-znn.zat (get-balance-at-block utxo 1043794))))

(defun utxos-at-block (blockheight &key (filterp (lambda (utxo balance) t)))
  (loop for utxo in *utxos*
        for balance = (get-balance-at-block utxo blockheight)
        when (funcall filterp utxo balance)
          collect (format nil "~A:~A" utxo (number-to-znn.zat balance))))

(defun make-balance-filter (min-balance)
  (lambda (utxo balance)
    (<= min-balance balance)))


(defun list-of-utxos-with-balance-greater-n-znn (n)
  (let ((zats (* n (expt 10 8))))
    (utxos-at-block (get-max-block-height) :filterp (make-balance-filter zats))))

;; Produce index.html
(ql:quickload :spinneret)

;; This takes a while!
;;(defparameter *znn>1* (list-of-utxos-with-balance-greater-n-znn 1))

 (defmacro with-page ((&key title) &body body)
   `(spinneret:with-html-string
      (:doctype)
      (:html
        (:head
         (:title ,title))
        (:body ,@body))))

(defun print-index.html (path)
  (with-open-file (stream (concatenate 'string path "/index.html")
                          :direction :output
                          :if-exists :supersede
                          :if-does-not-exist :create)
    (format stream "~A"
            (with-page (:title "ZNN-UTXOs with balance > 1 at block 1357233")
              (:ul (dolist (entry *znn>1*)
                     (:li (:code entry))))))))
